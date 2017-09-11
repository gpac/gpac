/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2017
 *					All rights reserved
 *
 *  This file is part of GPAC / ISOBMFF reader filter
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


#ifndef _ISMO_IN_H_
#define _ISMO_IN_H_

#include <gpac/constants.h>
#include <gpac/isomedia.h>
#include <gpac/filters.h>
#include <gpac/thread.h>

#ifndef GPAC_DISABLE_ISOM

/*
			reader module

*/

//#define DASH_USE_PULL

typedef struct
{
	//arguments
	char *src;


	//internal

	GF_Filter *filter;

	/*current channels*/
	GF_List *channels;

	/*input file*/
	GF_ISOFile *mov;
	u32 time_scale;
	u32 nb_playing;
	Bool is_local;


	/*remote file handling*/
#ifdef FILTER_FIXME
	GF_DownloadSession * dnload;
#endif
	u64 missing_bytes, last_size;

	u32 play_only_track_id;

	/*0: not fragmented - 1 fragmented - 2 fragmented and last fragment received*/
	u32 frag_type;
	Bool waiting_for_data, reset_frag_state;


	u32 pending_scalable_enhancement_segment_index;

	Bool use_memory;
	/*0: segment is not opened - 1: segment is opened but can be refreshed incomplete file) - 2: segment is fully parsed, no need for refresh*/
	u32 seg_opened;
	Bool drop_next_segment;
	Bool in_data_flush;
	u32 has_pending_segments, nb_force_flush;

	Bool clock_discontinuity;
	Bool disconnected;
	Bool no_order_check;

	u64 last_sender_ntp, cts_for_last_sender_ntp;
	Double remain_at_buffering_start;
	Bool buffering;
} ISOMReader;


typedef struct
{
	u32 track, track_id;
	/*base track if scalable media, 0 otherwise*/
	u32 base_track;
	u32 next_track;
	GF_FilterPid *pid;

	ISOMReader *owner;
	u64 duration;

	Bool wait_for_segment_switch;
	/*current sample*/
	GF_ISOSample *sample;
	GF_SLHeader current_slh;
	GF_Err last_state;

	Bool is_pulling;

	Bool has_edit_list;
	u32 sample_num;
	s64 dts_offset;
	Bool do_dts_shift_test;
	/*for edit lists*/
	u32 edit_sync_frame;
	u64 sample_time, start, end;
	Double speed;

	u32 time_scale;
	Bool to_init, is_playing, has_rap;
	u8 streamType;

	Bool is_encrypted, is_cenc;

	/*cache stuff*/
	u64 cache_seed_ts;
	u32 frame_cts_offset;
	u64 prev_dts, max_cts;
	GF_ISOSample *cache_sample;

	Bool buffering;
	u32 buffer_min, buffer_max;

	Bool disable_seek;
	u32 nalu_extract_mode;

	u32 last_sample_desc_index;
} ISOMChannel;

void isor_reset_reader(ISOMChannel *ch);
void isor_reader_get_sample(ISOMChannel *ch);
void isor_reader_release_sample(ISOMChannel *ch);

//ISOMChannel *isor_get_channel(ISOMReader *reader, GF_FilterPid *pid);
ISOMChannel *ISOR_CreateChannel(ISOMReader *read, GF_FilterPid *pid, u32 track);

/*uses nero chapter info and remaps to MPEG-4 OCI if no OCI present in descriptor*/
void isor_emulate_chapters(GF_ISOFile *file, GF_InitialObjectDescriptor *iod);

void isor_declare_objects(ISOMReader *read);

void isor_send_cenc_config(ISOMChannel *ch);

void isor_flush_data(ISOMReader *read, Bool check_buffer_level, Bool is_chunk_flush);

#endif /*GPAC_DISABLE_ISOM*/

#endif /*_ISMO_IN_H_*/

