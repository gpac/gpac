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
	//source data is completely available
	Bool input_loaded;
	//fragmented file to be refreshed before processing it
	Bool refresh_fragmented;

	u64 missing_bytes, last_size;

	u32 play_only_track_id;
	u32 play_only_first_media;
	/*0: not fragmented - 1 fragmented - 2 fragmented and last fragment received*/
	u32 frag_type;
	Bool waiting_for_data, reset_frag_state;


	u32 pending_scalable_enhancement_segment_index;

	Bool drop_next_segment;
	Bool in_data_flush;
	u32 has_pending_segments, nb_force_flush;

	Bool clock_discontinuity;
	Bool disconnected;
	Bool no_order_check;
	Bool moov_not_loaded;

	u64 last_sender_ntp, cts_for_last_sender_ntp;
	Bool is_partial_download, wait_for_source;

	u32 src_crc;
	u64 start_range, end_range;
	GF_FilterPid *pid;
} ISOMReader;


typedef struct
{
	u32 track, track_id;
	u32 item_id, item_idx;
	/*base track if scalable media, 0 otherwise*/
	u32 base_track;
	u32 next_track;
	GF_FilterPid *pid;

	ISOMReader *owner;
	u64 duration;

	/*current sample*/
	GF_ISOSample *static_sample;
	GF_ISOSample *sample;
	GF_SLHeader current_slh;
	GF_Err last_state;

	Bool has_edit_list;
	u32 sample_num;
	s64 dts_offset;
	Bool do_dts_shift_test;
	/*for edit lists*/
	u32 edit_sync_frame;
	u64 sample_time, start, end;
	Double speed;

	u32 time_scale;
	Bool to_init, has_rap;
	u32 play_state;
	u8 streamType;

	Bool is_encrypted, is_cenc;

	Bool disable_seek;
	u32 nalu_extract_mode;

	u32 last_sample_desc_index;

	char *sai_buffer;
	u32 sai_alloc_size, sai_buffer_size;
} ISOMChannel;

void isor_reset_reader(ISOMChannel *ch);
void isor_reader_get_sample(ISOMChannel *ch);
void isor_reader_release_sample(ISOMChannel *ch);

void isor_check_producer_ref_time(ISOMReader *read);

//ISOMChannel *isor_get_channel(ISOMReader *reader, GF_FilterPid *pid);
ISOMChannel *isor_create_channel(ISOMReader *read, GF_FilterPid *pid, u32 track, u32 item_id);

/*uses nero chapter info and remaps to MPEG-4 OCI if no OCI present in descriptor*/
void isor_emulate_chapters(GF_ISOFile *file, GF_InitialObjectDescriptor *iod);

void isor_declare_objects(ISOMReader *read);

void isor_reader_get_sample_from_item(ISOMChannel *ch);
void isor_set_crypt_config(ISOMChannel *ch);

#endif /*GPAC_DISABLE_ISOM*/

#endif /*_ISMO_IN_H_*/

