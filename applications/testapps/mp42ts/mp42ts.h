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
#include <gpac/esi.h>
#include <gpac/mpegts.h>
#include <gpac/avparse.h>
#include <gpac/thread.h>


typedef struct __m2ts_mux_program M2TS_Mux_Program;
typedef struct __m2ts_mux M2TS_Mux;

enum {
	LOG_NO_LOG = 0,
	LOG_PES = 1,
	LOG_SECTION = 2,
	LOG_TS = 3
};


typedef struct __m2ts_section {
	struct __m2ts_section *next;
	u8 *data;
	u32 length;
} M2TS_Mux_Section;

typedef struct __m2ts_table {
	struct __m2ts_table *next;
	u8 table_id;
	u8 version_number;
	struct __m2ts_section *section;
} M2TS_Mux_Table;

typedef struct
{
	u32 sec;
	u32 nanosec;
} M2TS_Time;


typedef struct __m2ts_mux_pck
{
	struct __m2ts_mux_pck *next;
	char *data;
	u32 data_len;
	u32 flags;
	u64 cts, dts;
} M2TS_Packet;


typedef struct __m2ts_mux_stream {
	struct __m2ts_mux_stream *next;

	u32 pid;
	u8 continuity_counter;
	struct __m2ts_mux_program *program;

	/*average stream bit-rate in bit/sec*/
	u32 bit_rate;
	
	/*multiplexer time - NOT THE PCR*/
	M2TS_Time time;

	/*table tools*/
	M2TS_Mux_Table *tables;
	/*total table sizes for bitrate estimation (PMT/PAT/...)*/
	u32 total_table_size;
	/* used for on-the-fly packetization of sections */
	M2TS_Mux_Table *current_table;
	M2TS_Mux_Section *current_section;
	u32 current_section_offset;
	u32 refresh_rate_ms;
	Bool table_needs_update;

	Bool (*process)(struct __m2ts_mux *muxer, struct __m2ts_mux_stream *stream);

	/*PES tools*/
	void *pes_packetizer;
	u32 mpeg2_stream_type;
	u32 mpeg2_stream_id;

	GF_ESIPacket pck;
	u32 pck_offset;

	struct __elementary_stream_ifce *ifce;
	Double ts_scale;

	/*packet fifo*/
	M2TS_Packet *pck_first, *pck_last;
	GF_Mutex *mx;
	/*avg bitrate compute*/
	u64 last_br_time;
	u32 bytes_since_last_time;

	/*MPEG-4 over MPEG-2*/
	u8 table_id;
	GF_SLHeader sl_header;
	//GF_SLConfig sl_config;
} M2TS_Mux_Stream;


struct __m2ts_mux_program {
	struct __m2ts_mux_program *next;

	struct __m2ts_mux *mux;
	u16 number;
	/*all streams but PMT*/
	M2TS_Mux_Stream *streams;
	/*PMT*/
	M2TS_Mux_Stream *pmt;
	/*pointer to PCR stream*/
	M2TS_Mux_Stream *pcr;

	Bool pcr_init;
	/*TS time at pcr init*/
	M2TS_Time pcr_init_ts_time;
	u64 pcr_init_time;

	GF_Descriptor *iod;
};

struct __m2ts_mux {
	M2TS_Mux_Program *programs;
	M2TS_Mux_Stream *pat;

	u16 ts_id;

	Bool needs_reconfig;
	Bool real_time;
	/*if set bit-rate won't be re-estimated*/
	Bool fixed_rate;

	/*output bit-rate in bit/sec*/
	u32 bit_rate;

	char dst_pck[188], null_pck[188];

	/*multiplexer time in micro-sec*/
	M2TS_Time time, init_ts_time;
	u32 init_sys_time;

	Bool eos_found;
	u32 pck_sent, last_br_time, avg_br;
	u64 tot_pck_sent, tot_pad_sent;

	Bool mpeg4_signaling;
};


enum
{
	GF_M2TS_STATE_IDLE,
	GF_M2TS_STATE_DATA,
	GF_M2TS_STATE_PADDING,
	GF_M2TS_STATE_EOS,
};

