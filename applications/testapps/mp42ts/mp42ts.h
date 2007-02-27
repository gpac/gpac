/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) ENST 2000-200X
 *					All rights reserved
 *
 *  This file is part of GPAC / mp42ts application
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
#include <gpac/isomedia.h>
#include <gpac/bifs.h>
#include <gpac/mpegts.h>
#include <gpac/avparse.h>

enum {
	LOG_NO_LOG = 0,
	LOG_PES = 1,
	LOG_SECTION = 2,
	LOG_TS = 3
};

/* max size includes first header, second header, payload and CRC */
enum {
	GF_M2TS_TABLE_ID_PAT			= 0x00,
	GF_M2TS_TABLE_ID_CAT			= 0x01, 
	GF_M2TS_TABLE_ID_PMT			= 0x02, 
	GF_M2TS_TABLE_ID_TSDT			= 0x03, /* max size for section 1024 */
	GF_M2TS_TABLE_ID_MPEG4_BIFS		= 0x04, /* max size for section 4096 */
	GF_M2TS_TABLE_ID_MPEG4_OD		= 0x05, /* max size for section 4096 */
	GF_M2TS_TABLE_ID_METADATA		= 0x06, 
	GF_M2TS_TABLE_ID_IPMP_CONTROL	= 0x07, 
	/* 0x08 - 0x37 reserved */
	/* 0x38 - 0x3D DSM-CC defined */
	GF_M2TS_TABLE_ID_DSM_CC_PRIVATE	= 0x3E, /* used for MPE (only, not MPE-FEC) */
	/* 0x3F DSM-CC defined */
	GF_M2TS_TABLE_ID_NIT_ACTUAL		= 0x40, /* max size for section 1024 */
	GF_M2TS_TABLE_ID_NIT_OTHER		= 0x41,
	GF_M2TS_TABLE_ID_SDT_ACTUAL		= 0x42, /* max size for section 1024 */
	/* 0x43 - 0x45 reserved */
	GF_M2TS_TABLE_ID_SDT_OTHER		= 0x46, /* max size for section 1024 */
	/* 0x47 - 0x49 reserved */
	GF_M2TS_TABLE_ID_BAT			= 0x4a, /* max size for section 1024 */
	/* 0x4b - 0x4d reserved */
	GF_M2TS_TABLE_ID_EIT_ACTUAL_PF	= 0x4E, /* max size for section 4096 */
	GF_M2TS_TABLE_ID_EIT_OTHER_PF	= 0x4F,
	/* 0x50 - 0x6f EIT SCHEDULE */
	GF_M2TS_TABLE_ID_TDT			= 0x70,
	GF_M2TS_TABLE_ID_RST			= 0x71, /* max size for section 1024 */
	GF_M2TS_TABLE_ID_ST 			= 0x72, /* max size for section 4096 */
	GF_M2TS_TABLE_ID_TOT			= 0x73,
	GF_M2TS_TABLE_ID_AI				= 0x74,
	GF_M2TS_TABLE_ID_CONT			= 0x75,
	GF_M2TS_TABLE_ID_RC				= 0x76,
	GF_M2TS_TABLE_ID_CID			= 0x77,
	GF_M2TS_TABLE_ID_MPE_FEC		= 0x78,
	GF_M2TS_TABLE_ID_RES_NOT		= 0x79,
	/* 0x7A - 0x7D reserved */
	GF_M2TS_TABLE_ID_DIT			= 0x7E,
	GF_M2TS_TABLE_ID_SIT			= 0x7F, /* max size for section 4096 */
	/* 0x80 - 0xfe reserved */
	/* 0xff reserved */
};

/*Events used by the MPEGTS muxer*/
enum
{
	GF_M2TS_EVT_PAT = 0,
	GF_M2TS_EVT_PMT,		
	GF_M2TS_EVT_SDT,		
	GF_M2TS_EVT_ES,
	GF_M2TS_EVT_SL_SECTION,
	/* Data from demuxer */
	GF_M2TS_EVT_DEMUX_DATA,
};

typedef struct
{
	u8 *data;
	u32 length;
} MP42TS_Buffer;

typedef struct M2TS_mux_stream
{
	u32 track_number;
	u32 mpeg2_es_pid;
	u32 mpeg2_pes_streamid;
	u32 mpeg4_es_id;

	u32 sample_number;
	u32 nb_samples;
	u32 nb_bytes_written;
	u32 continuity_counter;

	GF_ISOSample *sample;
	GF_SLConfig *SLConfig;
	GF_SLHeader *SLHeader;
	
	Bool SL_in_pes;
	u8 *sl_packet;
	u32 sl_packet_len;
	Bool SL_in_section;
	Bool repeat_section;
	u32 SL_section_version_number;
	GF_List *sl_section_ts_packets;
	
	GF_M2TS_PES *PES;

	u8 is_time_initialized;
	u64 stream_time;
	
	u32 MP2_type;
	u32 MP4_type;

	FILE *pes_out;

	Bool PCR;

	struct M2TS_muxer *muxer;
} M2TS_mux_stream;

typedef struct M2TS_muxer
{
	u32 TS_Rate;  // bps
	float PAT_interval; // s
	float PMT_interval; // s
	float SI_interval; //s
	
	u32 send_pat, send_pmt;

	/* ~ PCR */
	u64 muxer_time;
	
	/* M2TS_mux_stream List*/
	GF_List *streams;

	GF_List *pat_table; /* List of GF_M2TS_Program */

	GF_List *pat_ts_packet; /* List of Encoded TS packets corresponding to PAT */
	GF_List *sdt_ts_packet; /* List of Encoded TS packets corresponding to SDT */
	GF_List *pmt_ts_packet; /* List of List of Encoded TS packets corresponding to each PMT */

	/*user callback - MUST NOT BE NULL*/
	void (*on_event)(struct M2TS_muxer *muxer, u32 evt_type, void *par);
	/*private user data*/
	void *user;

	GF_ISOFile *mp4_in;
	FILE *ts_out;

	u32 insert_SI;
	Bool end;

	Bool use_sl;

	u32 log_level;
} M2TS_muxer;

typedef struct
{
	u32 *pid;
	u32 length;
} M2TS_pid_list;
