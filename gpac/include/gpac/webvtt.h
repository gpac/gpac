/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato, Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2013-2019
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

/*!
\file <gpac/webvtt.h>
\brief Helper functions for WebVTT parsing.
 */

/*!
\addtogroup wvtt_grp WebVTT parsing
\ingroup media_grp
\brief Helper functions for WebVTT parsing

This section documents the audio and video parsing functions of the GPAC framework.

@{
 */

/*! WebVTT types */
typedef enum {
	WEBVTT_ID,
	WEBVTT_SETTINGS,
	WEBVTT_PAYLOAD,
	WEBVTT_POSTCUE_TEXT,
	WEBVTT_PRECUE_TEXT,
} GF_WebVTTCuePropertyType;

/*! WebVTT timestamp information*/
typedef struct _webvtt_timestamp {
	u32 hour, min, sec, ms;
} GF_WebVTTTimestamp;
/*! gets webvtt timestamp
\param wvtt_ts the target WebVTT timestamp
\return timestamp in millisecond*/
u64 gf_webvtt_timestamp_get(GF_WebVTTTimestamp *wvtt_ts);
/*! sets webvtt timestamp
\param wvtt_ts the target WebVTT timestamp
\param value timestamp in millisecond
*/
void gf_webvtt_timestamp_set(GF_WebVTTTimestamp *wvtt_ts, u64 value);
/*! dumps webvtt timestamp
\param wvtt_ts the target WebVTT timestamp
\param dump the output file to write to
\param dump_hour if GF_TRUE, dumps hours
*/
void gf_webvtt_timestamp_dump(GF_WebVTTTimestamp *wvtt_ts, FILE *dump, Bool dump_hour);

/*! WebVTT cue structure*/
typedef struct _webvtt_cue
{
	GF_WebVTTTimestamp start;
	GF_WebVTTTimestamp end;
	char *id;
	char *settings;
	char *text;
	char *pre_text;
	char *post_text;

	Bool split;
	/* original times before split, if applicable */
	GF_WebVTTTimestamp orig_start;
	GF_WebVTTTimestamp orig_end;
} GF_WebVTTCue;
/*! destroys a WebVTT cue
\param cue the target WebVTT cue
*/
void gf_webvtt_cue_del(GF_WebVTTCue * cue);

#ifndef GPAC_DISABLE_VTT
/*! dumps webvtt sample boxes
\param dump the output file to write to
\param data the WebVTT ISOBMFF sample payload
\param dataLength the size of the payload in bytes
\param printLength set to the printed size in bytes
\return error if any
*/
GF_Err gf_webvtt_dump_header_boxed(FILE *dump, const u8 *data, u32 dataLength, u32 *printLength);
#endif

/*! dumps webvtt sample boxes
\param data the WebVTT ISOBMFF sample payload
\param dataLength the size of the payload in bytes
\param start the start time in milliseconds of the WebVTT cue
\param end the end time in milliseconds of the WebVTT cue
\return new list of cues, to destroy by caller
*/
GF_List *gf_webvtt_parse_cues_from_data(const u8 *data, u32 dataLength, u64 start, u64 end);

/*! @} */

#ifdef __cplusplus
}
#endif


#endif		/*_GF_THREAD_H_*/

