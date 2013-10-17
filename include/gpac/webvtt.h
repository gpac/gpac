/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2013-
 *					All rights reserved
 *
 *  This file is part of GPAC / WebVTT header 
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

#ifndef _GF_WEBVTT_H_
#define _GF_WEBVTT_H_

#ifdef __cplusplus
extern "C" {
#endif

/* WebVTT types */
typedef enum {
    WEBVTT_ID,
    WEBVTT_SETTINGS,
    WEBVTT_PAYLOAD,
    WEBVTT_TIME
} GF_WebVTTCuePropertyType;

typedef struct _webvtt_timestamp {
    u32 hour, min, sec, ms;
} GF_WebVTTTimestamp;
u64 gf_webvtt_timestamp_get(GF_WebVTTTimestamp *ts);
void gf_webvtt_timestamp_set(GF_WebVTTTimestamp *ts, u64 value);
void gf_webvtt_timestamp_dump(GF_WebVTTTimestamp *ts, FILE *dump, Bool dump_hour);

typedef struct _webvtt_cue
{
    GF_WebVTTTimestamp start;
    GF_WebVTTTimestamp end;
    char *id;
    char *settings;
    char *text;
    char *time;

    Bool split;
    /* original times before split, if applicable */
    GF_WebVTTTimestamp orig_start;
    GF_WebVTTTimestamp orig_end;
} GF_WebVTTCue;

void gf_webvtt_cue_del(GF_WebVTTCue * cue);

GF_Err gf_webvtt_dump_header_boxed(FILE *dump, const char *data, u32 dataLength, u32 *printLength);

#ifdef GPAC_HAS_SPIDERMONKEY
#include <gpac/internal/scenegraph_dev.h>

GF_Err gf_webvtt_js_addCue(GF_Node *node, const char *id, 
										  const char *start, const char *end,
										  const char *settings, 
										  const char *payload);
GF_Err gf_webvtt_js_removeCues(GF_Node *node);
#endif

#ifdef __cplusplus
}
#endif


#endif		/*_GF_THREAD_H_*/

