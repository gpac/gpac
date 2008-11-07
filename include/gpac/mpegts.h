/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Walid B.H - Jean Le Feuvre
 *    Copyright (c)2006-200X ENST - All rights reserved
 *
 *  This file is part of GPAC / MPEG2-TS sub-project
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


#ifndef _GF_MPEG_TS_H_
#define _GF_MPEG_TS_H_

#include <gpac/list.h>
#include <gpac/internal/odf_dev.h>

typedef struct tag_m2ts_demux GF_M2TS_Demuxer;
typedef struct tag_m2ts_es GF_M2TS_ES;
typedef struct tag_m2ts_section_es GF_M2TS_SECTION_ES;

/*Maximum number of streams in a TS*/
#define GF_M2TS_MAX_STREAMS	8192

/*MPEG-2 TS Media types*/
enum
{
	GF_M2TS_VIDEO_MPEG1				= 0x01,
	GF_M2TS_VIDEO_MPEG2				= 0x02,
	GF_M2TS_AUDIO_MPEG1				= 0x03,
	GF_M2TS_AUDIO_MPEG2				= 0x04, 
	GF_M2TS_PRIVATE_SECTION			= 0x05,
	GF_M2TS_PRIVATE_DATA			= 0x06,
	GF_M2TS_AUDIO_AAC				= 0x0F,
	GF_M2TS_VIDEO_MPEG4				= 0x10,
	GF_M2TS_AUDIO_LATM_AAC			= 0x11,

	GF_M2TS_SYSTEMS_MPEG4_PES		= 0x12,
	GF_M2TS_SYSTEMS_MPEG4_SECTIONS	= 0x13,

	GF_M2TS_VIDEO_H264				= 0x1B,

	GF_M2TS_AUDIO_AC3				= 0x81,
	GF_M2TS_AUDIO_DTS				= 0x8A,
	GF_M2TS_SUBTITLE_DVB			= 0x100,
};
/*returns readable name for given stream type*/
const char *gf_m2ts_get_stream_name(u32 streamType);

/*PES data framing modes*/
enum
{
	/*use data framing: recompute start of AUs (data frames)*/
	GF_M2TS_PES_FRAMING_DEFAULT,
	/*don't use data framing: all packets are raw PES packets*/
	GF_M2TS_PES_FRAMING_RAW,
	/*skip pes processing: all transport packets related to this stream are discarded*/
	GF_M2TS_PES_FRAMING_SKIP
};

/*PES packet flags*/
enum
{
	GF_M2TS_PES_PCK_RAP = 1,
	GF_M2TS_PES_PCK_AU_START = 1<<1,
	/*visual frame starting in this packet is an I frame or IDR (AVC/H264)*/
	GF_M2TS_PES_PCK_I_FRAME = 1<<2,
	/*visual frame starting in this packet is a P frame*/
	GF_M2TS_PES_PCK_P_FRAME = 1<<3,
	/*visual frame starting in this packet is a B frame*/
	GF_M2TS_PES_PCK_B_FRAME = 1<<4
};

/*Events used by the MPEGTS demuxer*/
enum
{
	/*PAT has been found (service connection) - no assoctiated parameter*/
	GF_M2TS_EVT_PAT_FOUND = 0,
	/*PAT has been updated - no assoctiated parameter*/
	GF_M2TS_EVT_PAT_UPDATE,
	/*repeated PAT has been found (carousel) - no assoctiated parameter*/
	GF_M2TS_EVT_PAT_REPEAT,
	/*PMT has been found (service tune-in) - assoctiated parameter: new PMT*/
	GF_M2TS_EVT_PMT_FOUND,
	/*repeated PMT has been found (carousel) - assoctiated parameter: updated PMT*/
	GF_M2TS_EVT_PMT_REPEAT,
	/*PMT has been changed - assoctiated parameter: updated PMT*/
	GF_M2TS_EVT_PMT_UPDATE,
	/*SDT has been received - assoctiated parameter: none*/
	GF_M2TS_EVT_SDT_FOUND,
	/*repeated SDT has been found (carousel) - assoctiated parameter: none*/
	GF_M2TS_EVT_SDT_REPEAT,
	/*SDT has been received - assoctiated parameter: none*/
	GF_M2TS_EVT_SDT_UPDATE,
	/*INT has been received - assoctiated parameter: none*/
	GF_M2TS_EVT_INT_FOUND,
	/*repeated INT has been found (carousel) - assoctiated parameter: none*/
	GF_M2TS_EVT_INT_REPEAT,
	/*INT has been received - assoctiated parameter: none*/
	GF_M2TS_EVT_INT_UPDATE,
	/*PES packet has been received - assoctiated parameter: PES packet*/
	GF_M2TS_EVT_PES_PCK,
	/*PCR has been received - assoctiated parameter: PES packet with no data*/
	GF_M2TS_EVT_PES_PCR,
	/*An MPEG-4 SL Packet has been received in a section - assoctiated parameter: SL packet */
	GF_M2TS_EVT_SL_PCK,
	/*An IP datagram has been received in a section - assoctiated parameter: IP datagram */
	GF_M2TS_EVT_IP_DATAGRAM,

	/*AAC config has been extracted - associated parameter: PES Packet with encoded M4ADecSpecInfo in its data
		THIS MUST BE CLEANED UP
	*/
	GF_M2TS_EVT_AAC_CFG,
#if 0
	/* An EIT message for the present or following event on this TS has been received */
	GF_M2TS_EVT_EIT_ACTUAL_PF,
	/* An EIT message for the schedule of this TS has been received */
	GF_M2TS_EVT_EIT_ACTUAL_SCHEDULE,
	/* An EIT message for the present or following event of an other TS has been received */
	GF_M2TS_EVT_EIT_OTHER_PF,
	/* An EIT message for the schedule of an other TS has been received */
	GF_M2TS_EVT_EIT_OTHER_SCHEDULE,
	/* A message to inform about the current date and time in the TS */
	GF_M2TS_EVT_TDT,
	/* A message to inform about the current time offset in the TS */
	GF_M2TS_EVT_TOT,
#endif
	/* A generic event message for EIT, TDT, TOT etc */
	GF_M2TS_EVT_DVB_GENERAL,
};

enum
{
	GF_M2TS_TABLE_START		= 1,
	GF_M2TS_TABLE_END		= 1<<1,
	GF_M2TS_TABLE_FOUND		= 1<<2,
	GF_M2TS_TABLE_UPDATE	= 1<<3,
	GF_M2TS_TABLE_REPEAT	= 1<<4,
};

typedef void (*gf_m2ts_section_callback)(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *es, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status); 

typedef struct __m2ts_demux_section
{
	unsigned char *data;
	u32 data_size;
} GF_M2TS_Section;

typedef struct __m2ts_demux_table
{
	struct __m2ts_demux_table *next;

	Bool is_init;

	/*table id*/
	u8 table_id;
	u16 ex_table_id;

	/*reassembler state*/
	u8 version_number;
	u8 last_version_number;

	u8 current_next_indicator;

	u8 section_number;
	u8 last_section_number;

	GF_List *sections;

} GF_M2TS_Table;


typedef struct GF_M2TS_SectionFilter
{
	/*section reassembler*/
	s16 cc;
	/*section buffer (max 4096)*/
	char *section;
	/*current section length as indicated in section header*/
	u16 length;
	/*number of bytes received from current section*/
	u16 received;
	/*error indiator when reaggregating sections*/
	u8 had_error;

	/*section->table aggregator*/
	GF_M2TS_Table *table;
	
	/* indicates that the section and last_section_number do not need to be checked */
	Bool process_individual;

	/* indicates that the section header with table id and extended table id ... is
	   not parsed by the TS demuxer and left for the application  */
	Bool direct_dispatch;

	gf_m2ts_section_callback process_section; 
} GF_M2TS_SectionFilter;



/*MPEG-2 TS program object*/
typedef struct 
{
	GF_List *streams;
	u32 pmt_pid;  
	u32 pcr_pid;
	u32 number;

	GF_InitialObjectDescriptor *pmt_iod;

	/*list of additional ODs found per program !! used by media importer only , refine this !! 
		this list is only created when MPEG-4 over MPEG-2 is detected
		the list AND the ODs contained in it are destroyed when destroying the demuxer
	*/
	GF_List *additional_ods;
	/*first dts found on this program - this is used by parsers, but not setup by the lib*/
	u64 first_dts;
} GF_M2TS_Program;

/*ES flags*/
enum
{
	/*ES is a section stream*/
	GF_M2TS_ES_IS_SECTION = 1,
	/*ES is an mpeg-4 flexmux stream*/
	GF_M2TS_ES_IS_FMC = 1<<1,
	/*ES is an mpeg-4 SL-packetized stream*/
	GF_M2TS_ES_IS_SL = 1<<2,
	/*ES is an mpeg-4 Object Descriptor SL-packetized stream*/
	GF_M2TS_ES_IS_MPEG4_OD = 1<<3,
	
	/*all flags above this mask are used by importers & co*/
	GF_M2TS_ES_STATIC_FLAGS_MASK = 0x0000FFFF,

	/*Flag used by importers*/
	GF_M2TS_ES_FIRST_DTS = 1<<20,
};

/*Abstract Section/PES stream object, only used for type casting*/
#define ABSTRACT_ES		\
			GF_M2TS_Program *program; \
			u32 flags; \
			u32 pid; \
			u32 stream_type; \
			u32 mpeg4_es_id; \
			void *user; \
			u64 first_dts;

struct tag_m2ts_es
{
	ABSTRACT_ES
};

/*INT object*/
typedef struct
{
	u32 id;
} GF_M2TS_INT;

struct tag_m2ts_section_es
{
	ABSTRACT_ES
	GF_M2TS_SectionFilter *sec;

	/* MPE Frame object, MPE-FEC related data */
	GF_M2TS_INT *ip_mac_not_table;	
};			

/*******************************************************************************/
typedef struct tag_m2ts_dvb_sub
{
	char language[3];
	u8 type;
	u16 composition_page_id;
	u16 ancillary_page_id;
} GF_M2TS_DVB_Subtitling_Descriptor;

typedef struct tag_m2ts_dvb_teletext
{
	char language[3];
	u8 type;
	u8 magazine_number;
	u8 page_number;
} GF_M2TS_DVB_Teletext_Descriptor;

/*MPEG-2 TS ES object*/
typedef struct tag_m2ts_pes
{
	ABSTRACT_ES
	u32 lang;

	/*object info*/
	u32 vid_w, vid_h, vid_par, aud_sr, aud_nb_ch;
	/*user private*/


	/*mpegts lib private - do not touch :)*/
	/*PES re-assembler*/
	unsigned char *data;
	u32 data_len, pes_len;
	Bool rap;
	u64 PTS, DTS;
	
	/*PES reframer - if NULL, pes processing is skiped*/
	u32 frame_state;
	void (*reframe)(struct tag_m2ts_demux *ts, struct tag_m2ts_pes *pes, u64 DTS, u64 CTS, unsigned char *data, u32 data_len);
	/*LATM stuff - should be moved out of mpegts*/
	unsigned char *buf;
	u32 buf_len;

	GF_M2TS_DVB_Subtitling_Descriptor sub;

} GF_M2TS_PES;

/*SDT information object*/
typedef struct
{
	u16 original_network_id;
	u16 transport_stream_id;
	u32 service_id;
	u32 EIT_schedule;
	u32 EIT_present_following;
	u32 running_status;
	u32 free_CA_mode;
	u8 service_type;
	unsigned char *provider, *service;
} GF_M2TS_SDT;

typedef struct
{
	u16 network_id;
	unsigned char *network_name;
	u16 original_network_id;
	u16 transport_stream_id;
	u16 service_id;
	u32 service_type;
	u32 logical_channel_number;
} GF_M2TS_NIT;

#define GF_M2TS_BASE_DESCRIPTOR u32 tag;

typedef struct {
	u8 content_nibble_level_1, content_nibble_level_2, user_nibble;
} GF_M2TS_DVB_Content_Descriptor;

typedef struct {
	char country_code[3];
	u8 value;
} GF_M2TS_DVB_Rating_Descriptor;

typedef struct {
	unsigned char lang[3];
	unsigned char *event_name, *event_text;
} GF_M2TS_DVB_Short_Event_Descriptor;

typedef struct {
	unsigned char *item;
	unsigned char *description;
} GF_M2TS_DVB_Extended_Event_Item;

typedef struct {
	unsigned char lang[3];
	u32 last;
	GF_List *items;
	unsigned char *text;
} GF_M2TS_DVB_Extended_Event_Descriptor;

/*EIT information objects*/
typedef struct
{
	u32 year, month, day, time;

	/* local time offset descriptor data */
	char country_code[3];
	u8 country_region_id;
	u8 local_time_offset_polarity;
	u16 local_time_offset;
	/*time_of_change*/
	u32 toc_year, toc_month, toc_day, toc_time;
	u16 next_time_offset;
} GF_M2TS_DateTime_Event;

typedef struct {
	u8 stream_content;
	u8 component_type;
	u8 component_tag;
	char language_code[3];
	unsigned char *text;
} GF_M2TS_Component;

typedef struct
{
	u16 event_id;
	GF_M2TS_DateTime_Event start;
	u32 duration;
	u8 running_status;
	u8 free_CA_mode;
	GF_List *short_events;
	GF_List *extended_events;
	GF_List *components;
	GF_List *contents;
	GF_List *ratings;
} GF_M2TS_EIT_Event;

typedef struct
{
	u32 original_network_id;
	u32 transport_stream_id;
	u16 service_id;
	GF_List *events;
} GF_M2TS_EIT;

void gf_m2ts_decode_mjd_date(u32 date, u32 *year, u32 *month, u32 *day);


/*MPEG-2 TS packet*/
typedef struct
{
	char *data;
	u32 data_len;
	u32 flags;
	u64 PTS, DTS;
	/*parent stream*/
	GF_M2TS_PES *stream;
} GF_M2TS_PES_PCK;

/*MPEG-4 SL packet from MPEG-2 TS*/
typedef struct
{
	char *data;
	u32 data_len;
	/*parent stream */
	GF_M2TS_ES *stream;
} GF_M2TS_SL_PCK;

/*MPEG-2 TS demuxer*/
struct tag_m2ts_demux
{
	GF_M2TS_ES *ess[GF_M2TS_MAX_STREAMS];
	GF_List *programs;
	/*keep it seperate for now - TODO check if we're sure of the order*/
	GF_List *SDTs;

	/*user callback - MUST NOT BE NULL*/
	void (*on_event)(struct tag_m2ts_demux *ts, u32 evt_type, void *par);
	/*private user data*/
	void *user;

	/*private resync buffer*/
	char *buffer;
	u32 buffer_size, alloc_size;
	/*default transport PID filters*/
	GF_M2TS_SectionFilter *pat, *nit, *sdt, *eit, *tdt_tot_st;

	/* Structure to hold all the INT tables if the TS contains IP streams */
	GF_List *ip_mac_not_tables;
	
	Bool has_4on2;
	/* analyser */
	FILE *pes_out;


};


GF_M2TS_Demuxer *gf_m2ts_demux_new();
void gf_m2ts_demux_del(GF_M2TS_Demuxer *ts);
void gf_m2ts_reset_parsers(GF_M2TS_Demuxer *ts);
GF_Err gf_m2ts_set_pes_framing(GF_M2TS_PES *pes, u32 mode);
GF_Err gf_m2ts_process_data(GF_M2TS_Demuxer *ts, char *data, u32 data_size);

u32 gf_m2ts_crc32_check(char *data, u32 len);

/*MPEG-2 Descriptor tags*/
enum
{
	/* ... */
	GF_M2TS_VIDEO_STREAM_DESCRIPTOR				= 0x02,
	GF_M2TS_AUDIO_STREAM_DESCRIPTOR				= 0x03,
	GF_M2TS_HIERARCHY_DESCRIPTOR				= 0x04,
	GF_M2TS_REGISTRATION_DESCRIPTOR				= 0x05,
	GF_M2TS_DATA_STREAM_ALIGNEMENT_DESCRIPTOR		= 0x06,
	GF_M2TS_TARGET_BACKGROUND_GRID_DESCRIPTOR		= 0x07,
	GF_M2TS_VIEW_WINDOW_DESCRIPTOR				= 0x08,
	GF_M2TS_CA_DESCRIPTOR					= 0x09,
	GF_M2TS_ISO_639_LANGUAGE_DESCRIPTOR			= 0x0A,
	/* ... */
	GF_M2TS_STD_DESCRIPTOR					= 0x17,
	/* ... */
	GF_M2TS_MPEG4_VIDEO_DESCRIPTOR				= 0x1B,
	GF_M2TS_MPEG4_AUDIO_DESCRIPTOR				= 0x1C,
	GF_M2TS_MPEG4_IOD_DESCRIPTOR				= 0x1D,
	GF_M2TS_MPEG4_SL_DESCRIPTOR				= 0x1E,
	GF_M2TS_MPEG4_FMC_DESCRIPTOR				= 0x1F,
	/* ... */
	GF_M2TS_AVC_VIDEO_DESCRIPTOR				= 0x28,
	/* ... */	
	GF_M2TS_AVC_TIMING_HRD_DESCRIPTOR			= 0x2A,
	/* ... */

	/* 0x2D - 0x3F - ISO/IEC 13818-6 values */
	/* 0x40 - 0xFF - User Private values */
	GF_M2TS_DVB_NETWORK_NAME_DESCRIPTOR			= 0x40,
	GF_M2TS_DVB_SERVICE_LIST_DESCRIPTOR			= 0x41,
	GF_M2TS_DVB_STUFFING_DESCRIPTOR				= 0x42,
	GF_M2TS_DVB_SAT_DELIVERY_SYSTEM_DESCRIPTOR		= 0x43,
	GF_M2TS_DVB_CABLE_DELIVERY_SYSTEM_DESCRIPTOR		= 0x44,
	GF_M2TS_DVB_VBI_DATA_DESCRIPTOR				= 0x45,
	GF_M2TS_DVB_VBI_TELETEXT_DESCRIPTOR			= 0x46,
	GF_M2TS_DVB_BOUQUET_NAME_DESCRIPTOR			= 0x47,
	GF_M2TS_DVB_SERVICE_DESCRIPTOR				= 0x48,
	GF_M2TS_DVB_COUNTRY_AVAILABILITY_DESCRIPTOR		= 0x49,
	GF_M2TS_DVB_LINKAGE_DESCRIPTOR				= 0x4A,
	GF_M2TS_DVB_NVOD_REFERENCE_DESCRIPTOR			= 0x4B,
	GF_M2TS_DVB_TIME_SHIFTED_SERVICE_DESCRIPTOR		= 0x4C,
	GF_M2TS_DVB_SHORT_EVENT_DESCRIPTOR			= 0x4D,
	GF_M2TS_DVB_EXTENDED_EVENT_DESCRIPTOR			= 0x4E,
	GF_M2TS_DVB_TIME_SHIFTED_EVENT_DESCRIPTOR		= 0x4F,
	GF_M2TS_DVB_COMPONENT_DESCRIPTOR			= 0x50,
	GF_M2TS_DVB_MOSAIC_DESCRIPTOR				= 0x51,
	GF_M2TS_DVB_STREAM_IDENTIFIER_DESCRIPTOR		= 0x52,
	GF_M2TS_DVB_CA_IDENTIFIER_DESCRIPTOR			= 0x53,
	GF_M2TS_DVB_CONTENT_DESCRIPTOR				= 0x54,
	GF_M2TS_DVB_PARENTAL_RATING_DESCRIPTOR			= 0x55,
	GF_M2TS_DVB_TELETEXT_DESCRIPTOR				= 0x56,
	/* ... */
	GF_M2TS_DVB_LOCAL_TIME_OFFSET_DESCRIPTOR		= 0x58,
	GF_M2TS_DVB_SUBTITLING_DESCRIPTOR			= 0x59,
	/* ... */
	GF_M2TS_DVB_DATA_BROADCAST_DESCRIPTOR			= 0x64,
	/* ... */
	GF_M2TS_DVB_DATA_BROADCAST_ID_DESCRIPTOR		= 0x66,
	/* ... */
	GF_M2TS_DVB_AC3_DESCRIPTOR				= 0x6A,
	
};

/* Reserved PID values */
enum {
	GF_M2TS_PID_PAT			= 0x0000,
	GF_M2TS_PID_CAT			= 0x0001,
	GF_M2TS_PID_TSDT		= 0x0002,
	/* reserved 0x0003 to 0x000F */ 
	GF_M2TS_PID_NIT_ST		= 0x0010,
	GF_M2TS_PID_SDT_BAT_ST		= 0x0011,
	GF_M2TS_PID_EIT_ST_CIT		= 0x0012,
	GF_M2TS_PID_RST_ST		= 0x0013,
	GF_M2TS_PID_TDT_TOT_ST		= 0x0014,
	GF_M2TS_PID_NET_SYNC		= 0x0015,
	GF_M2TS_PID_RNT			= 0x0016,
	/* reserved 0x0017 to 0x001B */ 
	GF_M2TS_PID_IN_SIG		= 0x001C,
	GF_M2TS_PID_MEAS		= 0x001D,
	GF_M2TS_PID_DIT			= 0x001E,
	GF_M2TS_PID_SIT			= 0x001F
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
	GF_M2TS_TABLE_ID_IPMP_CONTROL		= 0x07, 
	/* 0x08 - 0x37 reserved */
	/* 0x38 - 0x3D DSM-CC defined */
	GF_M2TS_TABLE_ID_DSM_CC_PRIVATE		= 0x3E, /* used for MPE (only, not MPE-FEC) */
	/* 0x3F DSM-CC defined */
	GF_M2TS_TABLE_ID_NIT_ACTUAL		= 0x40, /* max size for section 1024 */
	GF_M2TS_TABLE_ID_NIT_OTHER		= 0x41,
	GF_M2TS_TABLE_ID_SDT_ACTUAL		= 0x42, /* max size for section 1024 */
	/* 0x43 - 0x45 reserved */
	GF_M2TS_TABLE_ID_SDT_OTHER		= 0x46, /* max size for section 1024 */
	/* 0x47 - 0x49 reserved */
	GF_M2TS_TABLE_ID_BAT			= 0x4a, /* max size for section 1024 */
	/* 0x4b - 0x4d reserved */
	GF_M2TS_TABLE_ID_EIT_ACTUAL_PF		= 0x4E, /* max size for section 4096 */
	GF_M2TS_TABLE_ID_EIT_OTHER_PF		= 0x4F,
	/* 0x50 - 0x6f EIT SCHEDULE */
	GF_M2TS_TABLE_ID_EIT_SCHEDULE_MIN	= 0x50,
	GF_M2TS_TABLE_ID_EIT_SCHEDULE_ACTUAL_MAX= 0x5F,
	GF_M2TS_TABLE_ID_EIT_SCHEDULE_MAX	= 0x6F,

	GF_M2TS_TABLE_ID_TDT			= 0x70,
	GF_M2TS_TABLE_ID_RST			= 0x71, /* max size for section 1024 */
	GF_M2TS_TABLE_ID_ST 			= 0x72, /* max size for section 4096 */
	GF_M2TS_TABLE_ID_TOT			= 0x73,
	GF_M2TS_TABLE_ID_AI			= 0x74,
	GF_M2TS_TABLE_ID_CONT			= 0x75,
	GF_M2TS_TABLE_ID_RC			= 0x76,
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
	M2TS_ADAPTATION_AND_PAYLOAD 	= 3,
};


#define SECTION_HEADER_LENGTH 3 /* header till the last bit of the section_length field */
#define SECTION_ADDITIONAL_HEADER_LENGTH 5 /* header from the last bit of the section_length field to the payload */
#define	CRC_LENGTH 4

typedef struct
{
	u8 sync;
	u8 error;
	u8 payload_start;
	u8 priority;
	u16 pid;
	u8 scrambling_ctrl;
	u8 adaptation_field;
	u8 continuity_counter;
} GF_M2TS_Header;

typedef struct
{
	u32 discontinuity_indicator;
	u32 random_access_indicator;
	u32 priority_indicator;

	u32 PCR_flag;
	u64 PCR_base, PCR_ext;

	u32 OPCR_flag;
	u64 OPCR_base, OPCR_ext;

	u32 splicing_point_flag;
	u32 transport_private_data_flag;
	u32 adaptation_field_extension_flag;
/*	
	u32 splice_countdown;
	u32 transport_private_data_length;
	u32 adaptation_field_extension_length;
	u32 ltw_flag;
	u32 piecewise_rate_flag;
	u32 seamless_splice_flag;
	u32 ltw_valid_flag;
	u32 ltw_offset;
	u32 piecewise_rate;
	u32 splice_type;
	u32 DTS_next_AU;
*/
} GF_M2TS_AdaptationField;

typedef struct 
{
	u8 id;
	u16 pck_len;
	u8 data_alignment;
	u64 PTS, DTS;
	u8 hdr_data_len;
} GF_M2TS_PESHeader;

#endif	//_GF_MPEG_TS_H_
