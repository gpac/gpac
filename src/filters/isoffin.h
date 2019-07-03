/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
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

enum
{
	MP4DMX_SPLIT=0,
	MP4DMX_SPLIT_EXTRACTORS,
	MP4DMX_SINGLE,
};

typedef struct
{
	//options
	char *src;
	Bool allt, noedit, itt, itemid;
	u32 smode;
	u32 stsd;
	Bool expart;
	Bool alltk;
	u32 frame_size;
	char* tkid;
	Bool analyze;
	char *catseg;
	
	//internal

	GF_Filter *filter;

	/*current channels*/
	GF_List *channels;

	/*input file*/
	GF_ISOFile *mov;
	Bool extern_mov;
	u32 time_scale;
	u32 nb_playing;
	//source data is completely available
	Bool input_loaded;
	//fragmented file to be refreshed before processing it
	Bool refresh_fragmented;

	u64 missing_bytes, last_size;

	u32 play_only_track_id;
	u32 play_only_first_media;
	Bool full_segment_flush;
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

	Bool eos_signaled;
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

	GF_Err last_state;
	Bool sap_3, sap_4;
	s32 roll;

	Bool has_edit_list;
	u32 sample_num;
	s64 ts_offset;
/*
	s64 dts_offset;
	Bool do_dts_shift_test;
*/

	/*for edit lists*/
	u32 edit_sync_frame;
	u64 sample_time, last_rap_sample_time, start, end;
	Double speed;

	u32 time_scale;
	Bool to_init, has_rap;
	u32 play_state;
	u8 streamType;

	Bool is_encrypted, is_cenc;

	Bool disable_seek;
	u32 nalu_extract_mode;

	//channel sample state
	u32 last_sample_desc_index;
	u32 isLeading, dependsOn, dependedOn, redundant;
	u64 dts, cts;
	u8 skip_byte_block, crypt_byte_block;
	Bool is_protected;
	u8 constant_IV_size;
	bin128 constant_IV;
	u8 IV_size;
	u32 au_seq_num;
	u64 sender_ntp;
	u32 seek_flag;
	u32 au_duration;
	Bool set_disc;
	Bool cenc_state_changed;
	u64 isma_BSO;
	bin128 KID;
	Bool pck_encrypted;

	u8 *sai_buffer;
	u32 sai_alloc_size, sai_buffer_size;

	Bool check_avc_ps, check_hevc_ps;
	GF_HEVCConfig *hvcc;
	GF_AVCConfig *avcc;
	GF_BitStream *nal_bs;
	u32 dsi_crc;

	Bool needs_pid_reconfig;
} ISOMChannel;

void isor_reset_reader(ISOMChannel *ch);
void isor_reader_get_sample(ISOMChannel *ch);
void isor_reader_release_sample(ISOMChannel *ch);
void isor_update_channel_config(ISOMChannel *ch);

void isor_check_producer_ref_time(ISOMReader *read);

//ISOMChannel *isor_get_channel(ISOMReader *reader, GF_FilterPid *pid);
ISOMChannel *isor_create_channel(ISOMReader *read, GF_FilterPid *pid, u32 track, u32 item_id, Bool force_no_extractors);

GF_Err isor_declare_objects(ISOMReader *read);

void isor_reader_get_sample_from_item(ISOMChannel *ch);
void isor_set_crypt_config(ISOMChannel *ch);

void isor_reader_check_config(ISOMChannel *ch);

Bool isor_declare_item_properties(ISOMReader *read, ISOMChannel *ch, u32 item_idx);

#endif /*GPAC_DISABLE_ISOM*/

#endif /*_ISMO_IN_H_*/

