/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2013-
 *					All rights reserved
 *
 *  This file is part of GPAC / HTML Media Element header
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


#ifndef _GF_HTMLMEDIA_H_
#define _GF_HTMLMEDIA_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/internal/scenegraph_dev.h>

/*base SVG type*/
#include <gpac/nodes_svg.h>
/*dom events*/
#include <gpac/events.h>
/*dom text event*/
#include <gpac/utf.h>

#include <gpac/download.h>
#include <gpac/network.h>
#include <gpac/modules/service.h>
#include <gpac/xml.h>
#include <gpac/internal/terminal_dev.h>

#ifdef GPAC_HAS_SPIDERMONKEY
#include <gpac/internal/smjs_api.h>
#endif

typedef struct
{
    u32 nb_inst;
    /* Basic classes */
    GF_JSClass  arrayBufferClass;

    /*HTML Media classes*/
    GF_JSClass  htmlVideoElementClass;
    GF_JSClass  htmlAudioElementClass;
    GF_JSClass  htmlSourceElementClass;
    GF_JSClass  htmlTrackElementClass;
    GF_JSClass  htmlMediaElementClass;
    GF_JSClass  mediaControllerClass;
    GF_JSClass  audioTrackListClass;
    GF_JSClass  audioTrackClass;
    GF_JSClass  videoTrackListClass;
    GF_JSClass  videoTrackClass;
    GF_JSClass  textTrackListClass;
    GF_JSClass  textTrackClass;
    GF_JSClass  textTrackCueListClass;
    GF_JSClass  textTrackCueClass;
    GF_JSClass  timeRangesClass;
    GF_JSClass  trackEventClass;
    GF_JSClass  mediaErrorClass;

    /* Media Source Extensions */
    GF_JSClass  mediaSourceClass;
    GF_JSClass  sourceBufferClass;
    GF_JSClass  sourceBufferListClass;
    GF_JSClass  URLClass;

    /* Media Capture */
    GF_JSClass  mediaStreamClass;
    GF_JSClass  localMediaStreamClass;
    GF_JSClass  mediaStreamTrackClass;
    GF_JSClass  mediaStreamTrackListClass;
    GF_JSClass  navigatorUserMediaClass;
    GF_JSClass  navigatorUserMediaErrorClass;
} GF_HTML_MediaRuntime;

/************************************************************
 *
 *  HTML 5 Media Element
 *  http://www.whatwg.org/specs/web-apps/current-work/multipage/the-video-element.html#media-element
 *
 *************************************************************/

typedef enum
{
    MEDIA_ERR_ABORTED           = 1,
    MEDIA_ERR_NETWORK           = 2,
    MEDIA_ERR_DECODE            = 3,
    MEDIA_ERR_SRC_NOT_SUPPORTED = 4
} GF_HTML_MediaErrorCode;

typedef enum
{
    NETWORK_EMPTY               = 0,
    NETWORK_IDLE                = 1,
    NETWORK_LOADING             = 2,
    NETWORK_NO_SOURCE           = 3
} GF_HTML_NetworkState;

typedef enum
{
    HAVE_NOTHING                = 0,
    HAVE_METADATA               = 1,
    HAVE_CURRENT_DATA           = 2,
    HAVE_FUTURE_DATA            = 3,
    HAVE_ENOUGH_DATA            = 4
} GF_HTML_MediaReadyState;

typedef struct
{
    /* JavaScript context associated to this object */
    JSContext               *c;
    /* JavaScript counterpart */
    JSObject                *_this;

    GF_HTML_MediaErrorCode   code;
} GF_HTML_MediaError;

typedef struct _timerange
{
    /* JavaScript context associated to this object */
    JSContext               *c;
    /* JavaScript counterpart */
    JSObject                *_this;

    GF_List                 *times;
	u32						timescale;
} GF_HTML_MediaTimeRanges;

typedef enum {
    HTML_MEDIA_TRACK_TYPE_UNKNOWN = 0,
    HTML_MEDIA_TRACK_TYPE_AUDIO = 1,
    HTML_MEDIA_TRACK_TYPE_VIDEO = 2,
    HTML_MEDIA_TRACK_TYPE_TEXT  = 3
} GF_HTML_TrackType;

#define BASE_HTML_TRACK     \
    /* JavaScript context associated to this object */\
    JSContext               *c;\
    /* JavaScript counterpart */\
    JSObject                *_this;\
    /* GPAC-specific properties */\
    u32                     bin_id;    /* track id */\
    LPNETCHANNEL            channel;   /* channel object used by the terminal */\
    GF_ObjectDescriptor     *od;       /* MPEG-4 Object descriptor for this track */\
    GF_List                 *buffer;   /* List of MSE Packets */\
    u32						packet_index;   /* index of MSE Packets*/\
    GF_Mutex                *buffer_mutex;\
    Bool                    last_dts_set; \
    u64						last_dts; /* MSE  last decode timestamp (in timescale units)*/ \
    u32						last_dur; /* MSE  last frame duration (in timescale units)*/ \
    Bool                    highest_pts_set; \
    u64						highest_pts; /* MSE highest presentation timestamp (in timescale units)*/ \
    Bool                    needs_rap; /* MSE  need random access point flag */ \
    u32                     timescale; /* used by time stamps in MSE Packets */ \
    s64                     timestampOffset; /* MSE SourceBuffer value (in timescale units) */ \
    /* standard HTML properties */ \
    GF_HTML_TrackType        type;\
    char                    *id;\
    char                    *kind;\
    char                    *label;\
    char                    *language;\
    char                    *mime; \
    Bool                    enabled_or_selected; 

typedef struct
{
    BASE_HTML_TRACK
} GF_HTML_Track;

typedef struct
{
    BASE_HTML_TRACK
	JSFunction				*oncuechange;
    char                    *inBandMetadataTrackDispatchType;
    //GF_HTMLTextTrackMode    mode;
    //GF_HTMLTextTrackCueList cues;
    //GF_HTMLTextTrackCueList activeCues;
} GF_HTML_TextTrack;

#define BASE_HTML_TRACK_LIST     \
    /* JavaScript context associated to this object */\
    JSContext               *c;\
    /* JavaScript counterpart */\
    JSObject                *_this;\
    GF_List                 *tracks; \
	jsval					onchange; \
	jsval					onaddtrack; \
	jsval					onremovetrack; \
    u32                     selected_index;

typedef struct
{
    BASE_HTML_TRACK_LIST
} GF_HTML_TrackList;

typedef enum
{
    MEDIA_CONTROLLER_WAITING = 0,
    MEDIA_CONTROLLER_PLAYING = 1,
    MEDIA_CONTROLLER_ENDED   = 2
} GF_HTML_MediaControllerPlaybackState;

typedef struct
{
    /* JavaScript context associated to this object */
    JSContext               *c;
    /* JavaScript counterpart */
    JSObject                *_this;

    GF_HTML_MediaTimeRanges  *buffered;
    GF_HTML_MediaTimeRanges  *seekable;
    GF_HTML_MediaTimeRanges  *played;
    Bool                    paused;
    GF_HTML_MediaControllerPlaybackState playbackState;
    double                  defaultPlaybackRate;

    /* list of media elements using this media controller */
    GF_List                 *media_elements;
} GF_HTML_MediaController;

typedef struct
{
    /* JavaScript context associated to this object */
    JSContext               *c;
    /* JavaScript counterpart */
    JSObject                *_this;

    /* The audio or video node */
    GF_Node                 *node;

    /* error state */
    GF_HTML_MediaError       error;

    /* src: not stored in this structure, 
            using the value stored in the node ( see HTML 5 "must reflect the content of the attribute")*/
    /* currentSrc: the actual source used for the video (src attribute on video, audio or source elements) */
    char                    *currentSrc;
    /* crossOrigin: "must reflect the content of the attribute of the same name", use the node */
    /* networkState: retrieved dynamically from GPAC Service */
    /* preload: "must reflect the content of the attribute of the same name", use the node */
    GF_HTML_MediaTimeRanges  *buffered;
    /* ready state */
    /* readyState: retrieved from GPAC Media Object dynamically */
    Bool                    seeking;

    /* playback state */
    /* currentTime: retrieved from GPAC Media Object */
    /* duration: retrieved from GPAC Media Object */
    char                    *startDate;
    Bool                    paused;
    double                  defaultPlaybackRate;
    GF_HTML_MediaTimeRanges  *played;
    GF_HTML_MediaTimeRanges  *seekable;
    /* ended: retrieved from the state of GPAC Media Object */
    /* autoplay: "must reflect the content of the attribute of the same name", use the node */
    /* loop: "must reflect the content of the attribute of the same name", use the node */

    /* media controller*/
    /* mediaGroup: "must reflect the content of the attribute of the same name", use the node */
    GF_HTML_MediaController  *controller;

    /* controls*/
    /* controls: "must reflect the content of the attribute of the same name", use the node */
    /* volume: retrieved from GPAC Audio Input in GPAC Media Object */
    /* muted: retrieved from GPAC media object */
    /* defaultMuted: "must reflect the content of the attribute of with the name" muted */

    /* tracks*/
    GF_HTML_TrackList        audioTracks;
    GF_HTML_TrackList        videoTracks;
    GF_HTML_TrackList        textTracks;

	GF_DOMEventTarget		 *evt_target;
} GF_HTML_MediaElement;

typedef struct 
{
    /* JavaScript context used to create the JavaScript object below */
    JSContext               *c;

    /* JavaScript counterpart for this object*/
    JSObject                *_this;

	char    *data;
    u32     length;
    char    *url;
	Bool	is_init;

	/* used to do proper garbage collection between JS and Terminal */
    u32     reference_count;
} GF_HTML_ArrayBuffer;

/* 
 * TimeRanges 
 */
GF_HTML_MediaTimeRanges *gf_html_timeranges_new(u32 timescale);
GF_Err gf_html_timeranges_add_start(GF_HTML_MediaTimeRanges *timeranges, u64 start);
GF_Err gf_html_timeranges_add_end(GF_HTML_MediaTimeRanges *timeranges, u64 end);
void	gf_html_timeranges_reset(GF_HTML_MediaTimeRanges *range);
void	gf_html_timeranges_del(GF_HTML_MediaTimeRanges *range);
GF_HTML_MediaTimeRanges *gf_html_timeranges_intersection(GF_HTML_MediaTimeRanges *a, GF_HTML_MediaTimeRanges *b);
GF_HTML_MediaTimeRanges *gf_html_timeranges_union(GF_HTML_MediaTimeRanges *a, GF_HTML_MediaTimeRanges *b);

/*
 * HTML5 TrackList
 */
GF_HTML_Track *html_media_add_new_track_to_list(GF_HTML_TrackList *tracklist,
                                                       GF_HTML_TrackType type, const char *mime, Bool enable_or_selected,
                                                       const char *id, const char *kind, const char *label, const char *lang);
Bool html_media_tracklist_has_track(GF_HTML_TrackList *tracklist, const char *id);
GF_HTML_Track *html_media_tracklist_get_track(GF_HTML_TrackList *tracklist, const char *id);
void gf_html_tracklist_del(GF_HTML_TrackList *tlist);

/*
 * HTML5 Tracks
 */
GF_HTML_Track *gf_html_media_track_new(GF_HTML_TrackType type, const char *mime, Bool enable_or_selected,
                                       const char *id, const char *kind, const char *label, const char *lang);
void gf_html_track_del(GF_HTML_Track *track);

/* 
 * HTML5 Media Element
 */
GF_HTML_MediaElement *gf_html_media_element_new(GF_Node *media_node, GF_HTML_MediaController *mc);
void gf_html_media_element_del(GF_HTML_MediaElement *me);

void html_media_element_js_init(JSContext *c, JSObject *new_obj, GF_Node *n);

/* 
 * HTML5 Media Controller
 */
GF_HTML_MediaController *gf_html_media_controller_new();
void gf_html_media_controller_del(GF_HTML_MediaController *mc);

/* 
 * HTML5 Array Buffer
 */
GF_HTML_ArrayBuffer *gf_arraybuffer_new(char *data, u32 length);
JSObject *gf_arraybuffer_js_new(JSContext *c, char *data, u32 length, JSObject *parent);
void gf_arraybuffer_del(GF_HTML_ArrayBuffer *buffer, Bool del_js);

#ifdef __cplusplus
}
#endif

#endif	// _GF_HTMLMEDIA_H_
