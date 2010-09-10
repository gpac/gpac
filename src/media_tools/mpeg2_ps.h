/*
 * The contents of this file are subject to the Mozilla Public
 * License Version 1.1 (the "License"); you may not use this file
 * except in compliance with the License. You may obtain a copy of
 * the License at http://www.mozilla.org/MPL/
 * 
 * Software distributed under the License is distributed on an "AS
 * IS" basis, WITHOUT WARRANTY OF ANY KIND, either express or
 * implied. See the License for the specific language governing
 * rights and limitations under the License.
 * 
 * The Original Code is MPEG4IP.
 * 
 * The Initial Developer of the Original Code is Cisco Systems Inc.
 * Portions created by Cisco Systems Inc. are
 * Copyright (C) Cisco Systems Inc. 2004.  All Rights Reserved.
 * 
 * Contributor(s): 
 *		Bill May wmay@cisco.com
 */

/*
 * mpeg2_ps.h - API for mpeg2ps library
 */
#ifndef __MPEG2_PS_H__
#define __MPEG2_PS_H__ 1

#include <gpac/avparse.h>

#ifndef GPAC_DISABLE_MPEG2PS


#define MPEG2_PS_START      0x00000100
#define MPEG2_PS_START_MASK 0xffffff00
#define MPEG2_PS_PACKSTART  0x000001BA
#define MPEG2_PS_SYSSTART   0x000001BB
#define MPEG2_PS_END        0x000001B9


#define MPEG12_START_CODE_PREFIX          0x000001
#define MPEG12_PICTURE_START_CODE         0x00000100
#define MPEG12_SLICE_MIN_START            0x00000101
#define MPEG12_SLICE_MAX_START            0x000001af
#define MPEG12_USER_DATA_START_CODE       0x000001b2
#define MPEG12_SEQUENCE_START_CODE        0x000001b3
#define MPEG12_SEQUENCE_ERR_START_CODE    0x000001b4
#define MPEG12_EXT_START_CODE             0x000001b5
#define MPEG12_SEQUENCE_END_START_CODE    0x000001b7
#define MPEG12_GOP_START_CODE             0x000001b8

typedef struct mpeg2ps_ mpeg2ps_t;

typedef enum {
  TS_MSEC, 
  TS_90000,
} mpeg2ps_ts_type_t;

typedef enum {
  MPEG_AUDIO_MPEG = 0,
  MPEG_AUDIO_AC3 = 1,
  MPEG_AUDIO_LPCM = 2,
  MPEG_AUDIO_UNKNOWN = 3
} mpeg2ps_audio_type_t;

typedef enum {
  MPEG_VIDEO_MPEG1 = 0,
  MPEG_VIDEO_MPEG2 = 1,
  MPEG_VIDEO_UNKNOWN = 2
} mpeg2ps_video_type_t;

#ifdef __cplusplus 
extern "C" {
#endif
  /*
   * Interface routines for library.
   */
  /*
   * mpeg2ps_init() - use to start.  It will scan file for all streams
   * returns handle to use with rest of calls
   */
  mpeg2ps_t *mpeg2ps_init(const char *filename);

  /*
   * mpeg2ps_close - clean up - should be called after mpeg2ps_init
   */
  void mpeg2ps_close(mpeg2ps_t *ps);

  /*
   * mpeg2ps_get_max_time_msec - returns the max time of the longest stream
   */
  u64 mpeg2ps_get_max_time_msec(mpeg2ps_t *ps);
  /*
   * video stream functions
   */
  /*
   * mpeg2ps_get_video_stream_count - returns count of video streams in file
   */
  u32 mpeg2ps_get_video_stream_count(mpeg2ps_t *ps);
  /*
   * mpeg2ps_get_video_stream_name - returns display name for stream
   */
  const char *mpeg2ps_get_video_stream_name(mpeg2ps_t *ps, 
					    u32 streamno);
  /*
   * mpeg2ps_get_video_stream_type - returns enum type for stream
   */
  mpeg2ps_video_type_t mpeg2ps_get_video_stream_type(mpeg2ps_t *ps, 
						     u32 streamno);
  /*
   * these functions should be fairly self explanatory
   */
  u32 mpeg2ps_get_video_stream_width(mpeg2ps_t *ps, u32 streamno);
  u32 mpeg2ps_get_video_stream_height(mpeg2ps_t *ps, u32 streamno);
  u32 mpeg2ps_get_video_stream_aspect_ratio(mpeg2ps_t *ps, u32 streamno);
  double   mpeg2ps_get_video_stream_bitrate(mpeg2ps_t *ps, u32 streamno);
  double   mpeg2ps_get_video_stream_framerate(mpeg2ps_t *ps, u32 streamno);

  /*
   * mpeg2ps_get_video_frame - get the next video frame
   * returns false at end of stream
   * Inputs:
   *  ps - handle from above
   *  streamno - stream to read
   *  buffer - returns pointer to data.  Do not free
   *  buflen - frame length will be stored
   *  frame_type - if pointer given, frame type I-1, P-2, B-3 will be returned
   *  msec_timestamp - if pointer, time in msec since start will be given (dts)
   */
  Bool mpeg2ps_get_video_frame(mpeg2ps_t *ps, 
			       u32 streamno,
			       u8 **buffer, 
			       u32 *buflen,
			       u8 *frame_type,
			       mpeg2ps_ts_type_t ts_type,
			       u64 *timestamp);
  Bool mpeg2ps_seek_video_frame(mpeg2ps_t *ps, u32 streamno, 
				u64 msec_timestamp);

  /*
   * audio stream functions
   */
  /*
   * mpeg2ps_get_audio_stream_count - returns count of video streams in file
   */
  u32 mpeg2ps_get_audio_stream_count(mpeg2ps_t *ps);
  /*
   * mpeg2ps_get_audio_stream_name - returns display name for stream
   */
  const char *mpeg2ps_get_audio_stream_name(mpeg2ps_t *ps, u32 streamno);
  /*
   * mpeg2ps_get_audio_stream_type - returns enum type for stream
   */
  mpeg2ps_audio_type_t mpeg2ps_get_audio_stream_type(mpeg2ps_t *ps, u32 streamno);
  /*
   * these functions should be fairly self explanatory
   */
  u32 mpeg2ps_get_audio_stream_sample_freq(mpeg2ps_t *ps, u32 streamno);
  u32 mpeg2ps_get_audio_stream_channels(mpeg2ps_t *ps, u32 streamno);
  u32 mpeg2ps_get_audio_stream_bitrate(mpeg2ps_t *ps, u32 streamno);

  /*
   * mpeg2ps_get_audio_frame - get the next audio frame
   * returns false at end of stream
   * Inputs:
   *  ps - handle from above
   *  streamno - stream to read
   *  buffer - returns pointer to data.  Do not free
   *  buflen - frame length will be stored
   *  freq_timestamp - will return conversion of timestamp in units of 
   *                   sample_frequency.
   *  msec_timestamp - if pointer, time in msec since start will be given (dts)
   */
  Bool mpeg2ps_get_audio_frame(mpeg2ps_t *ps, 
			       u32 streamno,
			       u8 **buffer, 
			       u32 *buflen,
			       mpeg2ps_ts_type_t ts_type,
			       u32 *freq_timestamp,
			       u64 *msec_timestamp);
  Bool mpeg2ps_seek_audio_frame(mpeg2ps_t *ps, u32 streamno, 
				u64 msec_timestamp);


  /*
   * returns file size
   */
  u64 mpeg2ps_get_ps_size(mpeg2ps_t *ps);
  /*
   * returns position in file
   *  ps - handle from above
   *  streamno - stream to read
   */
  s64 mpeg2ps_get_video_pos(mpeg2ps_t *ps, u32 streamno);
  s64 mpeg2ps_get_audio_pos(mpeg2ps_t *ps, u32 streamno);

typedef void (*error_msg_func_t)(int loglevel,
				 const char *lib,
				 const char *fmt,
				 va_list ap);

  void mpeg2ps_set_loglevel(int loglevel);
  void mpeg2ps_set_error_func(error_msg_func_t func);


#endif /*GPAC_DISABLE_MPEG2PS*/

#ifdef __cplusplus
}
#endif

#endif
