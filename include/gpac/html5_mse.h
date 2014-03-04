/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2013-
 *					All rights reserved
 *
 *  This file is part of GPAC / HTML Media Source header
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


#ifndef _GF_HTMLMSE_H_
#define _GF_HTMLMSE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <gpac/setup.h>

#include <gpac/html5_media.h>
#include <gpac/internal/smjs_api.h>

typedef enum 
{
    MEDIA_SOURCE_READYSTATE_CLOSED = 0,
    MEDIA_SOURCE_READYSTATE_OPEN   = 1,
    MEDIA_SOURCE_READYSTATE_ENDED  = 2,
} GF_HTML_MediaSource_ReadyState;

typedef enum 
{
    MEDIA_SOURCE_APPEND_MODE_SEGMENTS	= 0,
    MEDIA_SOURCE_APPEND_MODE_SEQUENCE   = 1
} GF_HTML_MediaSource_AppendMode;

typedef enum 
{
    MEDIA_SOURCE_APPEND_STATE_WAITING_FOR_SEGMENT   = 0,
    MEDIA_SOURCE_APPEND_STATE_PARSING_INIT_SEGMENT  = 1,
    MEDIA_SOURCE_APPEND_STATE_PARSING_MEDIA_SEGMENT = 2
} GF_HTML_MediaSource_AppendState;

typedef struct
{
    /* Pointer back to the MediaSource object to which this source buffer is attached */
    struct _html_mediasource *mediasource;

    /* JavaScript counterpart for this object*/
    JSObject                *_this;

    /* MSE defined properties */
    Bool                    updating;
    GF_HTML_MediaTimeRanges *buffered;
    s64						timestampOffset;
    double                  appendWindowStart;
    double                  appendWindowEnd;
    u32                     timescale;

    GF_HTML_MediaSource_AppendState append_state;
    Bool                    buffer_full_flag;
	/* Mode used to append media data: 
	   - "segments" uses the timestamps in the media, 
	   - "sequence" ignores them and appends just after the previous data */
    GF_HTML_MediaSource_AppendMode   append_mode;

	/* time (in timescale units) of the first frame in the group */
    u64						group_start_timestamp;
    Bool                    group_start_timestamp_flag;
	/* time (in timescale units) of the frame end time (start + duration) in the group */
    u64						group_end_timestamp;
    Bool                    group_end_timestamp_set;

    Bool                    first_init_segment;

	/* times (in timescale units) of the frames to be removed */
    u64						remove_start;
    u64						remove_end;

    /*
	 * GPAC internal objects 
	 */

	/* Media tracks (GF_HTML_Track) associated to this source buffer */
    GF_List                 *tracks;
	/* Buffers to parse */
	GF_List					*input_buffer;
	/* We can only delete a buffer when we know it has been parsed, 
	   i.e. when the next buffer is asked for, 
	   so we need to keep the buffer in the meantime */
    void                    *prev_buffer;

	/* Media parser */
    GF_InputService         *parser;

	/* MPEG-4 Object descriptor as returned by the media parser */
    GF_ObjectDescriptor     *service_desc;  

    /* Boolean indicating that the parser has parsed the initialisation segment */
	Bool                    parser_connected;

	/* Threads used to asynchronously parse the buffer and remove media data */
	GF_List					*threads;
    GF_Thread               *parser_thread;
    GF_Thread               *remove_thread;

	/* Object used to fire JavaScript events to */
	GF_DOMEventTarget		*evt_target;
} GF_HTML_SourceBuffer;

typedef struct
{
    /* JavaScript counterpart for this object */
    JSObject                *_this;

    GF_List                 *list;

	struct _html_mediasource *parent;

	/* Object used to fire JavaScript events to */
	GF_DOMEventTarget		*evt_target;
} GF_HTML_SourceBufferList;

typedef enum
{
    DURATION_NAN        = 0,
    DURATION_INFINITY   = 1,
    DURATION_VALUE      = 2
} GF_HTML_MediaSource_DurationType;

typedef struct _html_mediasource
{
    /* JavaScript context associated to all the objects */
    JSContext               *c;

    /* JavaScript counterpart for this object*/
    JSObject                *_this;

	/* Used to determine if the object can be safely deleted (not used by JS, not used by the service) */
	u32 reference_count;

    GF_HTML_SourceBufferList sourceBuffers;
    GF_HTML_SourceBufferList activeSourceBuffers;

    double  duration;
    GF_HTML_MediaSource_DurationType    durationType;

    u32     readyState;

    /* URL created by the call to createObjectURL on this MediaSource*/
    char    *blobURI;

    /* GPAC Terminal Service object 
       it is associated to this MediaSource when the Media element uses the blobURI of this MediaSource
       should be NULL when the MediaSource is not open 
       we use only one service object for all sourceBuffers
       */
    GF_ClientService *service;

	/* SceneGraph to be used before the node is actually attached */
    GF_SceneGraph *sg;

    /* Node the MediaSource is attached to */
    GF_Node *node;

    /* object implementing Event Target Interface */
    GF_DOMEventTarget *evt_target;
} GF_HTML_MediaSource;

GF_HTML_MediaSource		*gf_mse_media_source_new();
void					gf_mse_mediasource_del(GF_HTML_MediaSource *ms, Bool del_js);
void					gf_mse_mediasource_open(GF_HTML_MediaSource *ms, struct _mediaobj *mo);
void					gf_mse_mediasource_close(GF_HTML_MediaSource *ms);
void					gf_mse_mediasource_end(GF_HTML_MediaSource *ms);
void					gf_mse_mediasource_add_source_buffer(GF_HTML_MediaSource *ms, GF_HTML_SourceBuffer *sb);

GF_HTML_SourceBuffer   *gf_mse_source_buffer_new(GF_HTML_MediaSource *mediasource);
void					gf_mse_source_buffer_set_timestampOffset(GF_HTML_SourceBuffer *sb, double d);
void					gf_mse_source_buffer_set_timescale(GF_HTML_SourceBuffer *sb, u32 timescale);
GF_Err                  gf_mse_source_buffer_load_parser(GF_HTML_SourceBuffer *sourcebuffer, const char *mime);
GF_Err					gf_mse_remove_source_buffer(GF_HTML_MediaSource *ms, GF_HTML_SourceBuffer *sb);
void                    gf_mse_source_buffer_del(GF_HTML_SourceBuffer *sb);
GF_Err                  gf_mse_source_buffer_abort(GF_HTML_SourceBuffer *sb);
void                    gf_mse_source_buffer_append_arraybuffer(GF_HTML_SourceBuffer *sb, GF_HTML_ArrayBuffer *buffer);
void                    gf_mse_source_buffer_update_buffered(GF_HTML_SourceBuffer *sb);
void					gf_mse_remove(GF_HTML_SourceBuffer *sb, double start, double end);
	
typedef struct
{
    char        *data;
    u32         size;
    GF_SLHeader sl_header;
    Bool        is_compressed;
    Bool        is_new_data;
    GF_Err      status;
} GF_MSE_Packet;

GF_Err gf_mse_proxy(GF_InputService *parser, GF_NetworkCommand *command);
void gf_mse_packet_del(GF_MSE_Packet *packet);

GF_Err gf_mse_track_buffer_get_next_packet(GF_HTML_Track *track,
											char **out_data_ptr, u32 *out_data_size, 
											GF_SLHeader *out_sl_hdr, Bool *sl_compressed, 
											GF_Err *out_reception_status, Bool *is_new_data);
GF_Err gf_mse_track_buffer_release_packet(GF_HTML_Track *track);

#ifdef __cplusplus
}
#endif

#endif	

