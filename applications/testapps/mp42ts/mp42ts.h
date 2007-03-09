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


/* ESI input control commands*/
enum
{
	/*forces a data flush from interface to dest (caller) - used for non-threaded interfaces
		corresponding parameter: unused
	*/
	GF_ESI_INPUT_DATA_FLUSH,
	/*pulls a COMPLETE AU from the stream
		corresponding parameter: pointer to a GF_ESIPacket to fill. The indut data_len in the packet is used to indicate any padding in bytes
	*/
	GF_ESI_INPUT_DATA_PULL,
	/*releases the currently pulled AU from the stream - AU cannot be pulled after that, unless seek happens
		corresponding parameter: unused
	*/
	GF_ESI_INPUT_DATA_RELEASE,
};
	
/*
	data packet flags
*/
enum
{
	/*data can be pulled from this stream*/
	GF_ESI_DATA_AU_START	=	1,
	GF_ESI_DATA_AU_END		=	1<<1,
	GF_ESI_DATA_AU_RAP		=	1<<2,
};

typedef struct __data_packet_ifce
{
	u32 flags;
	char *data;
	u32 data_len;
	/*DTS, CTS/PTS and duration expressed in media timescale*/
	u64 dts, cts;
	u32 duration;
} GF_ESIPacket;

struct __esi_video_info
{
	u32 width, height, par;
	Double FPS;
};
struct __esi_audio_info
{
	u32 sample_rate, nb_channels;
};

enum
{
	/*data can be pulled from this stream*/
	GF_ESI_AU_PULL_CAP	=	1,
	GF_ESI_STERAM_IS_OVER	=	1<<1,
};

typedef struct __elementary_stream_ifce 
{
	/*misc caps of the stream*/
	u32 caps;
	/*matches PID for MPEG2, ES_ID for MPEG-4*/
	u32 stream_id;
	/*MPEG-TS program number if any*/
	u16 program_number;
	/*MPEG-4 ST/OTIs*/
	u8 stream_type;
	u8 object_type_indication;
	/*stream 4CC for non-mpeg codecs, 0 otherwise (stream is identified through StreamType/ObjectType)*/
	u32 fourcc;
	/*packed 3-char language code (4CC with last byte ' ')*/
	u32 lang;
	/*media timescale*/
	u32 timescale;
	/*duration in ms - 0 if unknown*/
	Double duration;
	/*average bit rate in bit/sec - 0 if unknown*/
	u32 bit_rate;

	union {
		struct __esi_video_info video_info;
		struct __esi_audio_info audio_info;
	};

	/*input ES control from caller*/
	GF_Err (*input_ctrl)(struct __elementary_stream_ifce *_self, u32 ctrl_type, void *param);
	/*input user data of interface - usually set by interface owner*/
	void *input_udta;

	/*output ES control of destination*/
	GF_Err (*output_ctrl)(struct __elementary_stream_ifce *_self, u32 ctrl_type, void *param);
	/*output user data of interface - usually set during interface setup*/
	void *output_udta;

} GF_ESInterface;

typedef struct __service_ifce
{
	u32 type;

	/*input service control from caller*/
	GF_Err (*input_ctrl)(struct __service_ifce *_self, u32 ctrl_type, void *param);
	/*input user data of interface - usually set by interface owner*/
	void *input_udta;

	/*output service control of destination*/
	GF_Err (*output_ctrl)(struct __service_ifce *_self, u32 ctrl_type, void *param);
	/*output user data of interface - usually set during interface setup*/
	void *output_udta;

	GF_ESInterface **streams;
	u32 nb_streams;
} GF_ServiceInterface;

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

enum {
	M2TS_ADAPTATION_RESERVED	= 0,
	M2TS_ADAPTATION_NONE		= 1,
	M2TS_ADAPTATION_ONLY		= 2,
	M2TS_ADAPTATION_AND_PAYLOAD = 3,
};


#define SECTION_HEADER_LENGTH 3 /* header till the last bit of the section_length field */
#define SECTION_ADDITIONAL_HEADER_LENGTH 5 /* header from the last bit of the section_length field to the payload */
#define	CRC_LENGTH 4



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
} M2TS_Mux_Stream;


typedef struct __m2ts_mux_program {
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
} M2TS_Mux_Program;

typedef struct __m2ts_mux {
	M2TS_Mux_Program *programs;
	M2TS_Mux_Stream *pat;
	FILE *ts_out;

	u16 ts_id;

	Bool needs_reconfig;

	/*output bit-rate in bit/sec*/
	u32 bit_rate;

	char null_pck[188];

	/*multiplexer time in micro-sec*/
	M2TS_Time time;
} M2TS_Mux;


