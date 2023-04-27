/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

enum
{
	MP4DMX_XPS_AUTO=0,
	MP4DMX_XPS_KEEP,
	MP4DMX_XPS_REMOVE,
};

typedef struct
{
	//options
	char *src, *initseg;
	Bool allt, noedit, itt, itemid;
	u32 smode;
	u32 stsd;
	Bool expart;
	Bool alltk;
	u32 frame_size;
	char* tkid;
	Bool analyze;
	u32 xps_check;
	char *catseg;
	Bool sigfrag;
	Bool nocrypt, strtxt, nodata, lightp;
	u32 mstore_purge, mstore_samples, mstore_size;

	//internal

	GF_Filter *filter;

	/*current channels*/
	GF_List *channels;

	/*input file*/
	GF_ISOFile *mov;
	Bool extern_mov;
	u32 timescale;
	u32 nb_playing;
	//source data is completely available
	Bool input_loaded;
	//fragmented file to be refreshed before processing it
	Bool refresh_fragmented;
	Bool input_is_stop;
	u64 missing_bytes, last_size;

	Bool seg_name_changed;
	u32 play_only_track_id;
	u32 play_only_first_media;
	Bool full_segment_flush;
	/*0: not fragmented - 1 fragmented - 2 fragmented and last fragment received*/
	u32 frag_type;
	Bool waiting_for_data, reset_frag_state;

	Bool gfio_probe;

	u32 pending_scalable_enhancement_segment_index;

	Bool in_data_flush;
	u32 has_pending_segments, nb_force_flush;

	Bool disconnected;
	Bool no_order_check;
	u32 moov_not_loaded;
    Bool invalid_segment;
    
	u64 last_sender_ntp, ntp_at_last_sender_ntp, cts_for_last_sender_ntp;
	Bool is_partial_download, wait_for_source;

	u32 src_crc;
	u64 start_range, end_range;
	GF_FilterPid *pid;

	Bool eos_signaled;
	u32 mem_load_mode;
	u8 *mem_url;
	GF_Blob mem_blob;
	u32 mem_blob_alloc;
	u64 bytes_removed;
	u64 last_min_offset;
	GF_Err in_error;
	Bool force_fetch;
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
	u64 sample_data_offset, last_valid_sample_data_offset;
	GF_Err last_state;
	Bool sap_3;
	GF_ISOSampleRollType sap_4_type;
	s32 roll;
	u32 xps_mask;
	
	u32 sample_num, sample_last;
	s64 ts_offset;

	/*for edit lists*/
	u32 edit_sync_frame;
	u64 sample_time, last_rap_sample_time, start, end;
	Double speed, orig_start;

	u32 timescale;
	u32 clock_id;
	u8 streamType;
	//flags
	u8 has_edit_list;
	u8 last_has_tfrf;
	//0: not playing, 1: playing 2: playing but end of range reached
	u8 playing;
	u8 to_init, has_rap;
	u8 eos_sent;
	u8 initial_play_seen;
	u8 midrun_tune;
	u8 is_encrypted, is_cenc;
	u8 disable_seek, set_disc;
	u8 skip_next_play;
	u8 seek_flag;
	u8 check_avc_ps, check_hevc_ps, check_vvc_ps, check_mhas_pl;
	u8 needs_pid_reconfig;
	u8 check_has_rap;
	//0: no drop, 1: only keeps sap, 2: only keeps saps until next sap, then regular mode
	u8 sap_only;

	GF_ISONaluExtractMode nalu_extract_mode;

	//channel sample state
	u32 last_sample_desc_index;
	u32 isLeading, dependsOn, dependedOn, redundant;
	u64 dts, cts;
	u8 skip_byte_block, crypt_byte_block;
	u32 au_seq_num;
	u64 sender_ntp, ntp_at_server_ntp;

	u64 isma_BSO;
	Bool pck_encrypted;

	u32 key_info_crc;
	const GF_PropertyValue *cenc_ki;
	
	u8 *sai_buffer;
	u32 sai_alloc_size, sai_buffer_size;

	GF_HEVCConfig *hvcc;
	GF_AVCConfig *avcc;
	GF_VVCConfig *vvcc;
	u32 dsi_crc;
	u64 hint_first_tfdt;

	GF_FilterPacket *pck;
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

void isor_declare_pssh(ISOMChannel *ch);

void isor_set_sample_groups_and_aux_data(ISOMReader *read, ISOMChannel *ch, GF_FilterPacket *pck);

#endif /*GPAC_DISABLE_ISOM*/

#endif /*_ISMO_IN_H_*/

