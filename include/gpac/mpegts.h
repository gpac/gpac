/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril Concolato, Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2006-2012
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

/*!
 *	\file <gpac/mpegts.h>
 *	\brief MPEG-TS demultiplexer and multiplexer APIs
 */

/*!
 *	\addtogroup m2ts_grp MPEG-2 TS
 *	\ingroup media_grp
 *	\brief MPEG-TS demultiplexer and multiplexer APIs.
 *
 *This section documents the MPEG-TS demultiplexer and multiplexer APIs.
 *	@{
 */


#include <gpac/list.h>
#include <gpac/network.h>
#include <gpac/thread.h>
#include <gpac/internal/odf_dev.h>


#ifdef __cplusplus
extern "C" {
#endif

/* media types consts from media_import.c */
enum {
	GF_M2TS_META_ID3 	= GF_4CC('I','D','3',' '),
};



/*MPEG-2 Descriptor tags*/
enum
{
	/* ... */
	GF_M2TS_VIDEO_STREAM_DESCRIPTOR							= 0x02,
	GF_M2TS_AUDIO_STREAM_DESCRIPTOR							= 0x03,
	GF_M2TS_HIERARCHY_DESCRIPTOR							= 0x04,
	GF_M2TS_REGISTRATION_DESCRIPTOR							= 0x05,
	GF_M2TS_DATA_STREAM_ALIGNEMENT_DESCRIPTOR				= 0x06,
	GF_M2TS_TARGET_BACKGROUND_GRID_DESCRIPTOR				= 0x07,
	GF_M2TS_VIEW_WINDOW_DESCRIPTOR							= 0x08,
	GF_M2TS_CA_DESCRIPTOR									= 0x09,
	GF_M2TS_ISO_639_LANGUAGE_DESCRIPTOR						= 0x0A,
	GF_M2TS_DVB_IP_MAC_PLATFORM_NAME_DESCRIPTOR				= 0x0C,
	GF_M2TS_DVB_IP_MAC_PLATFORM_PROVIDER_NAME_DESCRIPTOR	= 0x0D,
	GF_M2TS_DVB_TARGET_IP_SLASH_DESCRIPTOR			= 0x0F,
	/* ... */
	GF_M2TS_DVB_STREAM_LOCATION_DESCRIPTOR        =0x13,
	/* ... */
	GF_M2TS_STD_DESCRIPTOR					= 0x17,
	/* ... */
	GF_M2TS_MPEG4_VIDEO_DESCRIPTOR				= 0x1B,
	GF_M2TS_MPEG4_AUDIO_DESCRIPTOR				= 0x1C,
	GF_M2TS_MPEG4_IOD_DESCRIPTOR				= 0x1D,
	GF_M2TS_MPEG4_SL_DESCRIPTOR				= 0x1E,
	GF_M2TS_MPEG4_FMC_DESCRIPTOR				= 0x1F,
	/* ... */
	GF_M2TS_METADATA_POINTER_DESCRIPTOR			= 0x25,
	GF_M2TS_METADATA_DESCRIPTOR					= 0x26,
	/* ... */
	GF_M2TS_AVC_VIDEO_DESCRIPTOR				= 0x28,
	/* ... */
	GF_M2TS_AVC_TIMING_HRD_DESCRIPTOR			= 0x2A,
	/* ... */
	GF_M2TS_SVC_EXTENSION_DESCRIPTOR			= 0x30,
	/* ... */
	GF_M2TS_MPEG4_ODUPDATE_DESCRIPTOR			= 0x35,

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
	GF_M2TS_DVB_PRIVATE_DATA_SPECIFIER_DESCRIPTOR = 0x5F,
	/* ... */
	GF_M2TS_DVB_DATA_BROADCAST_DESCRIPTOR			= 0x64,
	/* ... */
	GF_M2TS_DVB_DATA_BROADCAST_ID_DESCRIPTOR		= 0x66,
	/* ... */
	GF_M2TS_DVB_AC3_DESCRIPTOR				= 0x6A,
	/* ... */
	GF_M2TS_DVB_TIME_SLICE_FEC_DESCRIPTOR 		   = 0x77,
	/* ... */
	GF_M2TS_DVB_EAC3_DESCRIPTOR				= 0x7A,
	GF_M2TS_DVB_LOGICAL_CHANNEL_DESCRIPTOR = 0x83,
};

/* Reserved PID values */
enum {
	GF_M2TS_PID_PAT			= 0x0000,
	GF_M2TS_PID_CAT			= 0x0001,
	GF_M2TS_PID_TSDT		= 0x0002,
	/* reserved 0x0003 to 0x000F */
	GF_M2TS_PID_NIT_ST		= 0x0010,
	GF_M2TS_PID_SDT_BAT_ST	= 0x0011,
	GF_M2TS_PID_EIT_ST_CIT	= 0x0012,
	GF_M2TS_PID_RST_ST		= 0x0013,
	GF_M2TS_PID_TDT_TOT_ST	= 0x0014,
	GF_M2TS_PID_NET_SYNC	= 0x0015,
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
	GF_M2TS_TABLE_ID_IPMP_CONTROL	= 0x07,
	/* 0x08 - 0x37 reserved */
	/* 0x38 - 0x3D DSM-CC defined */
	GF_M2TS_TABLE_ID_DSM_CC_ENCAPSULATED_DATA		= 0x3A,
	GF_M2TS_TABLE_ID_DSM_CC_UN_MESSAGE				= 0x3B, /* used for MPE (only, not MPE-FEC) */
	GF_M2TS_TABLE_ID_DSM_CC_DOWNLOAD_DATA_MESSAGE	= 0x3C, /* used for MPE (only, not MPE-FEC) */
	GF_M2TS_TABLE_ID_DSM_CC_STREAM_DESCRIPTION		= 0x3D, /* used for MPE (only, not MPE-FEC) */
	GF_M2TS_TABLE_ID_DSM_CC_PRIVATE					= 0x3E, /* used for MPE (only, not MPE-FEC) */
	/* 0x3F DSM-CC defined */
	GF_M2TS_TABLE_ID_NIT_ACTUAL		= 0x40, /* max size for section 1024 */
	GF_M2TS_TABLE_ID_NIT_OTHER		= 0x41,
	GF_M2TS_TABLE_ID_SDT_ACTUAL		= 0x42, /* max size for section 1024 */
	/* 0x43 - 0x45 reserved */
	GF_M2TS_TABLE_ID_SDT_OTHER		= 0x46, /* max size for section 1024 */
	/* 0x47 - 0x49 reserved */
	GF_M2TS_TABLE_ID_BAT			= 0x4a, /* max size for section 1024 */
	/* 0x4b	reserved */
	GF_M2TS_TABLE_ID_INT			= 0x4c, /* max size for section 4096 */
	/* 0x4d reserved */

	GF_M2TS_TABLE_ID_EIT_ACTUAL_PF	= 0x4E, /* max size for section 4096 */
	GF_M2TS_TABLE_ID_EIT_OTHER_PF	= 0x4F,
	/* 0x50 - 0x6f EIT SCHEDULE */
	GF_M2TS_TABLE_ID_EIT_SCHEDULE_MIN	= 0x50,
	GF_M2TS_TABLE_ID_EIT_SCHEDULE_ACTUAL_MAX= 0x5F,
	GF_M2TS_TABLE_ID_EIT_SCHEDULE_MAX	= 0x6F,

	GF_M2TS_TABLE_ID_TDT			= 0x70, /* max size for section 1024 */
	GF_M2TS_TABLE_ID_RST			= 0x71, /* max size for section 1024 */
	GF_M2TS_TABLE_ID_ST 			= 0x72, /* max size for section 4096 */
	GF_M2TS_TABLE_ID_TOT			= 0x73, /* max size for section 1024 */
	GF_M2TS_TABLE_ID_AIT			= 0x74,
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

/*MPEG-2 TS Media types*/
enum
{
	GF_M2TS_VIDEO_MPEG1				= 0x01,
	GF_M2TS_VIDEO_MPEG2				= 0x02,
	GF_M2TS_AUDIO_MPEG1				= 0x03,
	GF_M2TS_AUDIO_MPEG2				= 0x04,
	GF_M2TS_PRIVATE_SECTION			= 0x05,
	GF_M2TS_PRIVATE_DATA			= 0x06,
	GF_M2TS_MHEG					= 0x07,
	GF_M2TS_13818_1_DSMCC			= 0x08,
	GF_M2TS_H222_1					= 0x09,
	GF_M2TS_13818_6_ANNEX_A			= 0x0A,
	GF_M2TS_13818_6_ANNEX_B			= 0x0B,
	GF_M2TS_13818_6_ANNEX_C			= 0x0C,
	GF_M2TS_13818_6_ANNEX_D			= 0x0D,
	GF_M2TS_13818_1_AUXILIARY		= 0x0E,
	GF_M2TS_AUDIO_AAC				= 0x0F,
	GF_M2TS_VIDEO_MPEG4				= 0x10,
	GF_M2TS_AUDIO_LATM_AAC			= 0x11,

	GF_M2TS_SYSTEMS_MPEG4_PES		= 0x12,
	GF_M2TS_SYSTEMS_MPEG4_SECTIONS	= 0x13,

	GF_M2TS_METADATA_PES			= 0x15,

	GF_M2TS_VIDEO_H264				= 0x1B,
	GF_M2TS_VIDEO_SVC				= 0x1F,
	GF_M2TS_VIDEO_HEVC				= 0x24,
	GF_M2TS_VIDEO_HEVC_TEMPORAL		= 0x25,
	GF_M2TS_VIDEO_MVCD				= 0x26,
	GF_M2TS_TEMI					= 0x27,
	GF_M2TS_VIDEO_SHVC				= 0x28,
	GF_M2TS_VIDEO_SHVC_TEMPORAL		= 0x29,
	GF_M2TS_VIDEO_MHVC				= 0x2A,
	GF_M2TS_VIDEO_MHVC_TEMPORAL		= 0x2B,

	GF_M2TS_GREEN					= 0x2C,
	GF_M2TS_MHAS_MAIN				= 0x2D,
	GF_M2TS_MHAS_AUX				= 0x2E,

	GF_M2TS_QUALITY_SEC				= 0x2F,
	GF_M2TS_MORE_SEC				= 0x30,

	GF_M2TS_VIDEO_HEVC_MCTS			= 0x31,

	GF_M2TS_VIDEO_DCII				= 0x80,
	GF_M2TS_AUDIO_AC3				= 0x81,
	GF_M2TS_AUDIO_DTS				= 0x8A,
	GF_M2TS_MPE_SECTIONS            = 0x90,
	GF_M2TS_SUBTITLE_DVB			= 0x100,

	/*internal use*/
	GF_M2TS_AUDIO_EC3				= 0x150,
	GF_M2TS_VIDEO_VC1				= 0x151,
	GF_M2TS_DVB_TELETEXT			= 0x152,
	GF_M2TS_DVB_VBI					= 0x153,
	GF_M2TS_DVB_SUBTITLE			= 0x154,
	GF_M2TS_METADATA_ID3_HLS		= 0x155,
};


/*MPEG-2 TS Registration codes types*/
enum
{
	GF_M2TS_RA_STREAM_AC3	= GF_4CC('A','C','-','3'),
	GF_M2TS_RA_STREAM_VC1	= GF_4CC('V','C','-','1'),

	GF_M2TS_RA_STREAM_GPAC	= GF_4CC('G','P','A','C')
};


/*MPEG-2 Descriptor tags*/
enum
{
	GF_M2TS_AFDESC_TIMELINE_DESCRIPTOR	= 0x04,
	GF_M2TS_AFDESC_LOCATION_DESCRIPTOR	= 0x05,
	GF_M2TS_AFDESC_BASEURL_DESCRIPTOR	= 0x06,
};

#define SECTION_HEADER_LENGTH 3 /* header till the last bit of the section_length field */
#define SECTION_ADDITIONAL_HEADER_LENGTH 5 /* header from the last bit of the section_length field to the payload */
#define	CRC_LENGTH 4



#ifndef GPAC_DISABLE_MPEG2TS

typedef struct tag_m2ts_demux GF_M2TS_Demuxer;
typedef struct tag_m2ts_es GF_M2TS_ES;
typedef struct tag_m2ts_section_es GF_M2TS_SECTION_ES;

/*Maximum number of streams in a TS*/
#define GF_M2TS_MAX_STREAMS	8192

/*Maximum number of service in a TS*/
#define GF_M2TS_MAX_SERVICES	65535

/*Maximum size of the buffer in UDP */
#ifdef WIN32
#define GF_M2TS_UDP_BUFFER_SIZE	0x80000
#else
//fixme - issues on linux and OSX with large stack size
//we need to change default stack size for TS thread
#define GF_M2TS_UDP_BUFFER_SIZE	0x40000
#endif

#define GF_M2TS_MAX_PCR	2576980377811ULL

/*returns readable name for given stream type*/
const char *gf_m2ts_get_stream_name(u32 streamType);

/*returns 1 if file is an MPEG-2 TS */
Bool gf_m2ts_probe_file(const char *fileName);

/*returns 1 if data is MPEG-2 TS */
Bool gf_m2ts_probe_data(const u8 *data, u32 size);

/*shifts all timing by the given value
@is_pes: array of GF_M2TS_MAX_STREAMS u8 set to 1 for PES PIDs to be restamped
*/
GF_Err gf_m2ts_restamp(u8 *buffer, u32 size, s64 ts_shift, u8 *is_pes);

/*PES data framing modes*/
enum
{
	/*skip pes processing: all transport packets related to this stream are discarded*/
	GF_M2TS_PES_FRAMING_SKIP,
	/*same as GF_M2TS_PES_FRAMING_SKIP but keeps internal PES buffer alive*/
	GF_M2TS_PES_FRAMING_SKIP_NO_RESET,
	/*don't use data framing: all packets are raw PES packets*/
	GF_M2TS_PES_FRAMING_RAW,

	//TODO - remove this one, we no longer reframe in the TS demuxer
	/*use data framing: recompute start of AUs (data frames)*/
	GF_M2TS_PES_FRAMING_DEFAULT,
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
	GF_M2TS_PES_PCK_B_FRAME = 1<<4,
	/*Possible PCR discontinuity from this packet on*/
	GF_M2TS_PES_PCK_DISCONTINUITY = 1<<5
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
	/*PCR has been received - associated parameter: PES packet with no data*/
	GF_M2TS_EVT_PES_PCR,
	/*PTS/DTS/PCR info - assoctiated parameter: PES packet with no data*/
	GF_M2TS_EVT_PES_TIMING,
	/*An MPEG-4 SL Packet has been received in a section - assoctiated parameter: SL packet */
	GF_M2TS_EVT_SL_PCK,
	/*An IP datagram has been received in a section - assoctiated parameter: IP datagram */
	GF_M2TS_EVT_IP_DATAGRAM,
	/*Duration has been estimated - assoctiated parameter: PES packet with no data, PTS is duration in msec*/
	GF_M2TS_EVT_DURATION_ESTIMATED,

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
#endif
	/* A message to inform about the current date and time in the TS */
	GF_M2TS_EVT_TDT,
	/* A message to inform about the current time offset in the TS */
	GF_M2TS_EVT_TOT,
	/* A generic event message for EIT, TDT, TOT etc */
	GF_M2TS_EVT_DVB_GENERAL,
	/* MPE / MPE-FEC frame extraction and IP datagrams decryptation */
	GF_M2TS_EVT_DVB_MPE,
	/*CAT has been found (service tune-in) - assoctiated parameter: new CAT*/
	GF_M2TS_EVT_CAT_FOUND,
	/*repeated CAT has been found (carousel) - assoctiated parameter: updated CAT*/
	GF_M2TS_EVT_CAT_REPEAT,
	/*PMT has been changed - assoctiated parameter: updated PMT*/
	GF_M2TS_EVT_CAT_UPDATE,
	/*AIT has been found (carousel) */
	GF_M2TS_EVT_AIT_FOUND,
	/*DSCM-CC has been found (carousel) */
	GF_M2TS_EVT_DSMCC_FOUND,

	/*a TEMI locator has been found or repeated*/
	GF_M2TS_EVT_TEMI_LOCATION,
	/*a TEMI timecode has been found*/
	GF_M2TS_EVT_TEMI_TIMECODE,

	GF_M2TS_EVT_EOS,
};

enum
{
	GF_M2TS_TABLE_START		= 1,
	GF_M2TS_TABLE_END		= 1<<1,
	GF_M2TS_TABLE_FOUND		= 1<<2,
	GF_M2TS_TABLE_UPDATE	= 1<<3,
	//both update and repeat flags may be set if data has changed
	GF_M2TS_TABLE_REPEAT	= 1<<4,
};

typedef void (*gf_m2ts_section_callback)(GF_M2TS_Demuxer *ts, GF_M2TS_SECTION_ES *es, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status);

typedef struct __m2ts_demux_section
{
	u8 *data;
	u32 data_size;
} GF_M2TS_Section;

typedef struct __m2ts_demux_table
{
	struct __m2ts_demux_table *next;

	u8 is_init;
	u8 is_repeat;

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

	u32 table_size;
} GF_M2TS_Table;


typedef struct GF_M2TS_SectionFilter
{
	/*section reassembler*/
	s16 cc;
	/*section buffer (max 4096)*/
	u8 *section;
	/*current section length as indicated in section header*/
	u16 length;
	/*number of bytes received from current section*/
	u16 received;

	/*section->table aggregator*/
	GF_M2TS_Table *table;

	/* indicates that the section and last_section_number do not need to be checked */
	Bool process_individual;

	/* indicates that the section header with table id and extended table id ... is
	   not parsed by the TS demuxer and left for the application  */
	Bool direct_dispatch;

	/* this field is used for AIT sections, to link the AIT with the program */
	u32 service_id;

	gf_m2ts_section_callback process_section;

	Bool demux_restarted;
} GF_M2TS_SectionFilter;

enum metadata_carriage {
	METADATA_CARRIAGE_SAME_TS		= 0,
	METADATA_CARRIAGE_DIFFERENT_TS	= 1,
	METADATA_CARRIAGE_PS			= 2,
	METADATA_CARRIAGE_OTHER			= 3
};

typedef struct tag_m2ts_metadata_pointer_descriptor {
	u16 application_format;
	u32 application_format_identifier;
	u8 format;
	u32 format_identifier;
	u8 service_id;
	Bool locator_record_flag;
	u32 locator_length;
	char *locator_data;
	enum metadata_carriage carriage_flag;
	u16 program_number;
	u16 ts_location;
	u16 ts_id;
	u8 *data;
	u32 data_size;
} GF_M2TS_MetadataPointerDescriptor;

typedef struct
{
	u32 timeline_id;
	//for now we only support one URL announcement
	const char *external_URL;
	Bool is_announce, is_splicing;
	Bool reload_external;
	Double activation_countdown;
} GF_M2TS_TemiLocationDescriptor;

typedef struct
{
	u32 timeline_id;
	u32 media_timescale;
	u64 media_timestamp;
	u64 pes_pts;
	Bool force_reload;
	Bool is_paused;
	Bool is_discontinuity;
	u64 ntp;
} GF_M2TS_TemiTimecodeDescriptor;


/*MPEG-2 TS program object*/
typedef struct
{
	GF_M2TS_Demuxer *ts;

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

	/* Last PCR value received for this program and associated packet number */
	u64 last_pcr_value;
	u32 last_pcr_value_pck_number;
	/* PCR value before the last received one for this program and associated packet number
	used to compute PCR interpolation value*/
	u64 before_last_pcr_value;
	u32 before_last_pcr_value_pck_number;

	/*for hybrid use-cases we need to know if TDT has already been processed*/
	Bool tdt_found;

	u32 pid_playing;
	Bool is_scalable;

	GF_M2TS_MetadataPointerDescriptor *metadata_pointer_descriptor;

	/*continuity counter check for pure PCR PIDs*/
	s16 pcr_cc;
} GF_M2TS_Program;

/*ES flags*/
enum
{
	/*ES is a PES stream*/
	GF_M2TS_ES_IS_PES = 1,
	/*ES is a section stream*/
	GF_M2TS_ES_IS_SECTION = 1<<1,
	/*ES is an mpeg-4 flexmux stream*/
	GF_M2TS_ES_IS_FMC = 1<<2,
	/*ES is an mpeg-4 SL-packetized stream*/
	GF_M2TS_ES_IS_SL = 1<<3,
	/*ES is an mpeg-4 Object Descriptor SL-packetized stream*/
	GF_M2TS_ES_IS_MPEG4_OD = 1<<4,
	/*ES is a DVB MPE stream*/
	GF_M2TS_ES_IS_MPE = 1<<5,
	/*stream is used to send PCR to upper layer*/
	GF_M2TS_INHERIT_PCR = 1<<6,
	/*signals the stream is used to send the PCR, but is not the original PID carrying it*/
	GF_M2TS_FAKE_PCR = 1<<7,
	/*signals the stream type is a gpac codec id*/
	GF_M2TS_GPAC_CODEC_ID = 1<<8,

	/*all flags above this mask are used by importers & co*/
	GF_M2TS_ES_STATIC_FLAGS_MASK = 0x0000FFFF,

	/*always send sections regardless of their version_number*/
	GF_M2TS_ES_SEND_REPEATED_SECTIONS = 1<<16,
	/*Flag used by importers*/
	GF_M2TS_ES_FIRST_DTS = 1<<17,

	/*flag used to signal next discontinuity on stream should be ignored*/
	GF_M2TS_ES_IGNORE_NEXT_DISCONTINUITY = 1<<18,

	/*Flag used by importers/readers to mark streams that have been seen already in PMT process (update/found)*/
	GF_M2TS_ES_ALREADY_DECLARED = 1<<19
};

/*Abstract Section/PES stream object, only used for type casting*/
#define ABSTRACT_ES		\
			GF_M2TS_Program *program; \
			u32 flags; \
			u32 pid; \
			u32 stream_type; \
			u32 mpeg4_es_id; \
			GF_SLConfig *slcfg; \
			s16 component_tag; \
			void *user; \
			u64 first_dts; \
			u32 service_id;

struct tag_m2ts_es
{
	ABSTRACT_ES
};


typedef struct
{
	u8 id;
	u16 pck_len;
	u8 data_alignment;
	u64 PTS, DTS;
	u8 hdr_data_len;
} GF_M2TS_PESHeader;

struct tag_m2ts_section_es
{
	ABSTRACT_ES
	GF_M2TS_SectionFilter *sec;
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

typedef struct tag_m2ts_metadata_descriptor {
	u16 application_format;
	u32 application_format_identifier;
	u8 format;
	u32 format_identifier;
	u8 service_id;
	u8 decoder_config_flags;
	Bool dsmcc_flag;
	u8 service_id_record_length;
	char *service_id_record;
	u8 decoder_config_length;
	u8 *decoder_config;
	u8 decoder_config_id_length;
	u8 *decoder_config_id;
	u8 decoder_config_service_id;
} GF_M2TS_MetadataDescriptor;

/*MPEG-2 TS ES object*/
typedef struct tag_m2ts_pes
{
	ABSTRACT_ES
	/*continuity counter check*/
	s16 cc;
	u32 lang;

	/*object info*/
	u32 vid_w, vid_h, vid_par, aud_sr, aud_nb_ch, aud_aac_obj_type, aud_aac_sr_idx;

	u32 depends_on_pid;

	/*user private*/


	/*mpegts lib private - do not touch :)*/
	/*PES re-assembler*/
	u8 *pck_data;
	/*amount of bytes allocated for data */
	u32 pck_alloc_len;
	/*amount of bytes received in the current PES packet (NOT INCLUDING ANY PENDING BYTES)*/
	u32 pck_data_len;
	/*size of the PES packet being received, as indicated in pes header length field - can be 0 if unknown*/
	u32 pes_len;
	Bool rap;
	u64 PTS, DTS;
	u32 pes_end_packet_number;
	/*bytes not consumed from previous PES - shall be less than 9*/
	u8 *prev_data;
	/*number of bytes not consumed from previous PES - shall be less than 9*/
	u32 prev_data_len;

	u32 pes_start_packet_number;
	/* PCR info related to the PES start */
	/* Last PCR value received for this program and associated packet number */
	u64 last_pcr_value;
	u32 last_pcr_value_pck_number;
	/* PCR value before the last received one for this program and associated packet number
	used to compute PCR interpolation value*/
	u64 before_last_pcr_value;
	u32 before_last_pcr_value_pck_number;


	/*PES reframer - if NULL, pes processing is skiped*/
	/*returns the number of bytes NOT consummed from the input data buffer - these bytes are kept when reassembling the next PES packet*/
	u32 (*reframe)(struct tag_m2ts_demux *ts, struct tag_m2ts_pes *pes, Bool same_pts, u8 *data, u32 data_len, GF_M2TS_PESHeader *hdr);

	/*used by several reframers to store their parsing state*/
	u32 frame_state;
	/*LATM stuff - should be moved out of mpegts*/
	u8 *buf, *reassemble_buf;
	u32 buf_len;
	u32 reassemble_len, reassemble_alloc;
	u64 prev_PTS;

	GF_M2TS_DVB_Subtitling_Descriptor sub;
	GF_M2TS_MetadataDescriptor *metadata_descriptor;

	//pointer to last received temi
	u8 *temi_tc_desc;
	u32 temi_tc_desc_len, temi_tc_desc_alloc_size;

	//last decoded temi (may be one ahead of time as the last received TEMI)
	GF_M2TS_TemiTimecodeDescriptor temi_tc;
	Bool temi_pending;
} GF_M2TS_PES;

enum
{
	GF_M2_STREAMID_PROGRAM_STREAM_MAP = 0xBC,
	GF_M2_STREAMID_PADDING = 0xBE,
	GF_M2_STREAMID_PRIVATE_2 = 0xBF,
	GF_M2_STREAMID_ECM = 0xF0,
	GF_M2_STREAMID_EMM = 0xF1,
	GF_M2_STREAMID_PROGRAM_STREAM_DIRECTORY = 0xFF,
	GF_M2_STREAMID_DSMCC = 0xF2,
	GF_M2_STREAMID_H222_TYPE_E = 0xF8
};

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
	char *provider, *service;
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

typedef struct
{
	u16 year;
	u8 month;
	u8 day;
	u8 hour;
	u8 minute;
	u8 second;
} GF_M2TS_TDT_TOT;

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
	time_t unix_time;

	/* local time offset descriptor data (unused ...) */
	char country_code[3];
	u8 country_region_id;
	s32 local_time_offset_seconds;
	time_t unix_next_toc;
	s32 next_time_offset_seconds;

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
	time_t unix_start;
	time_t unix_duration;


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
	u8 table_id;
} GF_M2TS_EIT;

/*MPEG-2 TS packet*/
typedef struct
{
	u8 *data;
	u32 data_len;
	u32 flags;
	u64 PTS, DTS;
	/*parent stream*/
	GF_M2TS_PES *stream;
} GF_M2TS_PES_PCK;

/*MPEG-4 SL packet from MPEG-2 TS*/
typedef struct
{
	u8 *data;
	u32 data_len;
	u8 version_number;
	/*parent stream */
	GF_M2TS_ES *stream;
} GF_M2TS_SL_PCK;

/*MPEG-2 TS demuxer*/
struct tag_m2ts_demux
{
	GF_M2TS_ES *ess[GF_M2TS_MAX_STREAMS];
	GF_List *programs;
	u32 nb_prog_pmt_received;
	Bool all_prog_pmt_received;
	Bool all_prog_processed;
	/*keep it seperate for now - TODO check if we're sure of the order*/
	GF_List *SDTs;
	GF_M2TS_TDT_TOT *TDT_time; /*UTC time from both TDT and TOT (we currently ignore local offset)*/

	/*user callback - MUST NOT BE NULL*/
	void (*on_event)(struct tag_m2ts_demux *ts, u32 evt_type, void *par);
	/*private user data*/
	void *user;

	/*private resync buffer*/
	u8 *buffer;
	u32 buffer_size, alloc_size;
	/*default transport PID filters*/
	GF_M2TS_SectionFilter *pat, *cat, *nit, *sdt, *eit, *tdt_tot;

	Bool has_4on2;
	/* analyser */
	FILE *pes_out;

	//when set, only pmt and PAT are parsed
	Bool seek_mode;
	Bool prefix_present;
	Bool direct_mpe;

	Bool dvb_h_demux;
	Bool notify_pes_timing;

	/*user callback - MUST NOT BE NULL*/
	void (*on_mpe_event)(struct tag_m2ts_demux *ts, u32 evt_type, void *par);
	/* Structure to hold all the INT tables if the TS contains IP streams */
	struct __gf_dvb_mpe_ip_platform *ip_platform;

	u32 pck_number;

	/*remote file handling - created and destroyed by user*/
	struct __gf_download_session *dnload;

	const char *dvb_channels_conf_path;

	/*AIT*/
	GF_List* ChannelAppList;

	/*Carousel*/
	Bool process_dmscc;
	char* dsmcc_root_dir;
	GF_List* dsmcc_controler;

	Bool table_reset;
};

GF_M2TS_Demuxer *gf_m2ts_demux_new();
void gf_m2ts_demux_del(GF_M2TS_Demuxer *ts);
void gf_m2ts_reset_parsers(GF_M2TS_Demuxer *ts);
void gf_m2ts_reset_parsers_for_program(GF_M2TS_Demuxer *ts, GF_M2TS_Program *prog);
GF_Err gf_m2ts_set_pes_framing(GF_M2TS_PES *pes, u32 mode);
void gf_m2ts_es_del(GF_M2TS_ES *es, GF_M2TS_Demuxer *ts);
GF_Err gf_m2ts_process_data(GF_M2TS_Demuxer *ts, u8 *data, u32 data_size);
u32 gf_dvb_get_freq_from_url(const char *channels_config_path, const char *url);
void gf_m2ts_demux_dmscc_init(GF_M2TS_Demuxer *ts);

GF_M2TS_SDT *gf_m2ts_get_sdt_info(GF_M2TS_Demuxer *ts, u32 program_id);

Bool gf_m2ts_crc32_check(u8 *data, u32 len);


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


#endif /*GPAC_DISABLE_MPEG2TS*/


#ifndef GPAC_DISABLE_MPEG2TS_MUX

/*!
 *	\addtogroup esi_grp ES Interface
 *	\ingroup media_grp
 *	\brief Basic stream interface API used by MPEG-2 TS muxer.
 *
 *This section documents the draft ES interface used by the MPEG-2 TS muxer.
 *	@{
 */

/* ESI input control commands*/
enum
{
	/*forces a data flush from interface to dest (caller) - used for non-threaded interfaces
		corresponding parameter: unused
	*/
	GF_ESI_INPUT_DATA_FLUSH,
	/*pulls a COMPLETE AU from the stream
		corresponding parameter: pointer to a GF_ESIPacket to fill. The input data_len in the packet is used to indicate any padding in bytes
	*/
	GF_ESI_INPUT_DATA_PULL,
	/*releases the currently pulled AU from the stream - AU cannot be pulled after that, unless seek happens
		corresponding parameter: unused
	*/
	GF_ESI_INPUT_DATA_RELEASE,

	/*destroys any allocated resource by the stream interface*/
	GF_ESI_INPUT_DESTROY,
};

/* ESI output control commands*/
enum
{
	/*forces a data flush from interface to dest (caller) - used for non-threaded interfaces
		corresponding parameter: GF_ESIPacket
	*/
	GF_ESI_OUTPUT_DATA_DISPATCH
};

/*
	data packet flags
*/
enum
{
	GF_ESI_DATA_AU_START	=	1,
	GF_ESI_DATA_AU_END		=	1<<1,
	GF_ESI_DATA_HAS_CTS		=	1<<2,
	GF_ESI_DATA_HAS_DTS		=	1<<3,
	GF_ESI_DATA_REPEAT		=	1<<4,
};

typedef struct __data_packet_ifce
{
	u16 flags;
	u8 sap_type;
	u8 *data;
	u32 data_len;
	/*DTS, CTS/PTS and duration expressed in media timescale*/
	u64 dts, cts;
	u32 duration;
	u32 pck_sn;
	/*MPEG-4 stuff*/
	u32 au_sn;
	/*for packets using ISMACrypt/OMA/3GPP based crypto*/
	u32 isma_bso;

	u8 *mpeg2_af_descriptors;
	u32 mpeg2_af_descriptors_size;
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
	/*no more data to expect from this stream*/
	GF_ESI_STREAM_IS_OVER	=	1<<2,
	/*stream is not signaled through MPEG-4 Systems (OD stream) */
	GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS =	1<<3,
	/*stream is not signaled through MPEG-4 Systems (OD stream) */
	GF_ESI_AAC_USE_LATM =	1<<4,
};

typedef struct __elementary_stream_ifce
{
	/*misc caps of the stream*/
	u32 caps;
	/*matches PID for MPEG2, ES_ID for MPEG-4*/
	u32 stream_id;
	/*MPEG-4 ST/OTIs*/
	u8 stream_type;
	u32 codecid;
	/*packed 3-char language code (4CC with last byte ' ')*/
	u32 lang;
	/*media timescale*/
	u32 timescale;
	/*duration in ms - 0 if unknown*/
	Double duration;
	/*average bit rate in bit/sec - 0 if unknown*/
	u32 bit_rate;
	/*repeat rate in ms for carrouseling - 0 if no repeat*/
	u32 repeat_rate;

	u8 *decoder_config;
	u32 decoder_config_size;

	/* MPEG-4 SL Config if any*/
	GF_SLConfig *sl_config;

	/*input ES control from caller*/
	GF_Err (*input_ctrl)(struct __elementary_stream_ifce *_self, u32 ctrl_type, void *param);
	/*input user data of interface - usually set by interface owner*/
	void *input_udta;

	/*output ES control of destination*/
	GF_Err (*output_ctrl)(struct __elementary_stream_ifce *_self, u32 ctrl_type, void *param);
	/*output user data of interface - usually set during interface setup*/
	void *output_udta;

	u32 depends_on_stream;

} GF_ESInterface;

/*! @} */

/*
	MPEG-2 TS Multiplexer
*/

enum {
	GF_M2TS_ADAPTATION_RESERVED	= 0,
	GF_M2TS_ADAPTATION_NONE		= 1,
	GF_M2TS_ADAPTATION_ONLY		= 2,
	GF_M2TS_ADAPTATION_AND_PAYLOAD 	= 3,
};



typedef struct __m2ts_mux_program GF_M2TS_Mux_Program;
typedef struct __m2ts_mux GF_M2TS_Mux;

typedef struct __m2ts_section {
	struct __m2ts_section *next;
	u8 *data;
	u32 length;
} GF_M2TS_Mux_Section;

typedef struct __m2ts_table {
	struct __m2ts_table *next;
	u8 table_id;
	u8 version_number;
	struct __m2ts_section *section;
} GF_M2TS_Mux_Table;

typedef struct
{
	u32 sec;
	u32 nanosec;
} GF_M2TS_Time;


typedef struct __m2ts_mux_pck
{
	struct __m2ts_mux_pck *next;
	u8 *data;
	u32 data_len;
	u16 flags;
	u8 sap_type;
	u64 cts, dts;
	u32 duration;

	u8 *mpeg2_af_descriptors;
	u32 mpeg2_af_descriptors_size;
} GF_M2TS_Packet;


typedef struct __m2ts_mux_stream {
	struct __m2ts_mux_stream *next;

	u32 pid;
	u8 continuity_counter;
	struct __m2ts_mux_program *program;

	/*average stream bit-rate in bit/sec*/
	u32 bit_rate;

	/*multiplexer time - NOT THE PCR*/
	GF_M2TS_Time time;

	GF_M2TS_Time next_time;
	Bool pcr_only_mode;

	/*table tools*/
	GF_M2TS_Mux_Table *tables;
	u8 initial_version_number;
	/*total table sizes for bitrate estimation (PMT/PAT/...)*/
	u32 total_table_size;
	/* used for on-the-fly packetization of sections */
	GF_M2TS_Mux_Table *current_table;
	GF_M2TS_Mux_Section *current_section;
	u32 current_section_offset;
	u32 refresh_rate_ms;
	Bool table_needs_update;
	Bool table_needs_send;
	Bool force_single_au;
	Bool set_initial_disc;
	Bool force_reg_desc;

	/*minimal amount of bytes we are allowed to copy frome next AU in the current PES. If no more than this
	is available in PES, don't copy from next*/
	u32 min_bytes_copy_from_next;
	/*process PES or table update/framing
	returns the priority of the stream,  0 meaning not scheduled, 1->N highest priority sent first*/
	u32 (*process)(struct __m2ts_mux *muxer, struct __m2ts_mux_stream *stream);

	/*PES tools*/
	void *pes_packetizer;
	u32 mpeg2_stream_type;
	u32 mpeg2_stream_id;
	u32 scheduling_priority;

	GF_ESIPacket curr_pck; /*current packet being processed - does not belong to the packet fifo*/
	u32 pck_offset;
	u32 next_payload_size, copy_from_next_packets, next_next_payload_size;
	u32 pes_data_len, pes_data_remain;
	Bool force_new;
	Bool discard_data;

	u16 next_pck_flags;
	u8 next_pck_sap;
	u64 next_pck_cts, next_pck_dts;

	u32 reframe_overhead;

	Bool start_pes_at_rap, prevent_two_au_start_in_pes;

	struct __elementary_stream_ifce *ifce;
	GF_Fraction ts_scale;

	/*packet fifo*/
	GF_M2TS_Packet *pck_first, *pck_last;
	/*packet reassembler (PES packets are most of the time full frames)*/
	GF_M2TS_Packet *pck_reassembler;
	//mutex if enabled
	GF_Mutex *mx;
	/*avg bitrate compute*/
	u64 last_br_time;
	u32 bytes_since_last_time, pes_since_last_time;
	u64 last_dts;
	/*MPEG-4 over MPEG-2*/
	u8 table_id;
	GF_SLHeader sl_header;

	u32 last_aac_time;
	/*list of GF_M2TSDescriptor to add to the MPEG-2 stream. By default set to NULL*/
	GF_List *loop_descriptors;

	//used when dashing
	u32 pck_sap_type;
	u64 pck_sap_time; //=pts
} GF_M2TS_Mux_Stream;

enum {
	GF_M2TS_MPEG4_SIGNALING_NONE = 0,
	GF_M2TS_MPEG4_SIGNALING_FULL,
	/*MPEG-4 over MPEG-2 profile where PES media streams may be refered to by the scene without SL-packetization*/
	GF_M2TS_MPEG4_SIGNALING_SCENE
};

typedef struct __m2ts_base_descriptor
{
	u8 tag;
	u8 data_len;
	u8 *data;
} GF_M2TSDescriptor;

struct __m2ts_mux_program {
	struct __m2ts_mux_program *next;

	struct __m2ts_mux *mux;
	u16 number;
	/*all streams but PMT*/
	GF_M2TS_Mux_Stream *streams;
	/*PMT*/
	GF_M2TS_Mux_Stream *pmt;
	/*pointer to PCR stream*/
	GF_M2TS_Mux_Stream *pcr;

	/*TS time at pcr init*/
	GF_M2TS_Time ts_time_at_pcr_init;
	u64 pcr_init_time, num_pck_at_pcr_init;
	u64 last_pcr;
	u64 last_dts;
	//high res clock at last PCR
	u64 sys_clock_at_last_pcr;
	u64 nb_pck_last_pcr;
	u64 initial_ts;
	Bool initial_ts_set;
	Bool pcr_init_time_set;
	u32 pcr_offset;
	Bool initial_disc_set;

	GF_Descriptor *iod;
	/*list of GF_M2TSDescriptor to add to the program descriptor loop. By default set to NULL, if non null will be reset and destroyed upon cleanup*/
	GF_List *loop_descriptors;
	u32 mpeg4_signaling;
	Bool mpeg4_signaling_for_scene_only;

	char *name, *provider;

	s32 max_media_skip;
	u32 cts_offset;
};

enum
{
	GF_SEG_BOUNDARY_NONE=0,
	GF_SEG_BOUNDARY_START,
	GF_SEG_BOUNDARY_FORCE_PAT,
	GF_SEG_BOUNDARY_FORCE_PMT,
	GF_SEG_BOUNDARY_FORCE_PCR,
};

/*AU packing per pes configuration*/
typedef enum
{
	/*only audio AUs are packed in a single PES, video and systems are not (recommended default)*/
	GF_M2TS_PACK_AUDIO_ONLY=0,
	/*never pack AUs in a single PES*/
	GF_M2TS_PACK_NONE,
	/*always try to pack AUs in a single PES*/
	GF_M2TS_PACK_ALL
} GF_M2TS_PackMode;

struct __m2ts_mux {
	GF_M2TS_Mux_Program *programs;
	GF_M2TS_Mux_Stream *pat;
	GF_M2TS_Mux_Stream *sdt;

	u16 ts_id;

	Bool needs_reconfig;

	/* used to indicate that the input data is pushed to the muxer (i.e. not read from a file)
	or that the output data is sent on sockets (not written to a file) */
	Bool real_time;

	/* indicates if the multiplexer shall target a fix bit rate (monitoring timing and produce padding packets)
	   or if the output stream will contain only input data*/
	Bool fixed_rate;

	/*output bit-rate in bit/sec*/
	u32 bit_rate;

	/*init value for PCRs on all streams if 0, random value is used*/
	u64 init_pcr_value;

	u32 pcr_update_ms;

	char dst_pck[188], null_pck[188];

	/*multiplexer time, incremented each time a packet is sent
	  used to monitor the sending of muxer related data (PAT, ...) */
	GF_M2TS_Time time;

	/* Time of the muxer when the first call to process is made (first packet sent?) */
	GF_M2TS_Time init_ts_time;

	/* System time high res when the muxer is started */
	u64 init_sys_time;

	Bool force_pat;

	GF_M2TS_PackMode au_pes_mode;

	Bool enable_forced_pcr;

	Bool eos_found;
	u64 last_br_time_us;
	u32 pck_sent_over_br_window;
	u64 tot_pck_sent, tot_pad_sent, tot_pes_pad_bytes;


	u32 average_birate_kbps;

	Bool flush_pes_at_rap;
	/*cf enum above*/
	u32 force_pat_pmt_state;


	GF_BitStream *pck_bs;
	//used when dashing, set to TRUE if the packet output is the first packet of a SAP AU
	Bool sap_inserted;
	u64 sap_time;
	u32 sap_type;
	u64 last_pts;
	u32 last_pid;
};




enum
{
	GF_M2TS_STATE_IDLE,
	GF_M2TS_STATE_DATA,
	GF_M2TS_STATE_PADDING,
	GF_M2TS_STATE_EOS,
};

#define GF_M2TS_PSI_DEFAULT_REFRESH_RATE	200
/*!
 * mux_rate en kbps
 */
GF_M2TS_Mux *gf_m2ts_mux_new(u32 mux_rate, u32 pat_refresh_rate, Bool real_time);
void gf_m2ts_mux_del(GF_M2TS_Mux *mux);
//sets max interval between two PCR. Default/max interval is 100 ms
void gf_m2ts_mux_set_pcr_max_interval(GF_M2TS_Mux *muxer, u32 pcr_update_ms);
GF_M2TS_Mux_Program *gf_m2ts_mux_program_add(GF_M2TS_Mux *muxer, u32 program_number, u32 pmt_pid, u32 pmt_refresh_rate, u32 pcr_offset, Bool mpeg4_signaling, u32 pmt_version, Bool initial_disc);
GF_M2TS_Mux_Stream *gf_m2ts_program_stream_add(GF_M2TS_Mux_Program *program, GF_ESInterface *ifce, u32 pid, Bool is_pcr, Bool force_pes_mode, Bool needs_mutex);
void gf_m2ts_mux_update_config(GF_M2TS_Mux *mux, Bool reset_time);
//removes stream from program, triggering PMT update as well
void gf_m2ts_program_stream_remove(GF_M2TS_Mux_Stream *stream);

GF_M2TS_Mux_Program *gf_m2ts_mux_program_find(GF_M2TS_Mux *muxer, u32 program_number);
u32 gf_m2ts_mux_program_count(GF_M2TS_Mux *muxer);
u32 gf_m2ts_mux_program_get_stream_count(GF_M2TS_Mux_Program *prog);
u32 gf_m2ts_mux_program_get_pmt_pid(GF_M2TS_Mux_Program *prog);
u32 gf_m2ts_mux_program_get_pcr_pid(GF_M2TS_Mux_Program *prog);

const u8 *gf_m2ts_mux_process(GF_M2TS_Mux *muxer, u32 *status, u32 *usec_till_next);
u32 gf_m2ts_get_sys_clock(GF_M2TS_Mux *muxer);
u32 gf_m2ts_get_ts_clock(GF_M2TS_Mux *muxer);

GF_Err gf_m2ts_mux_use_single_au_pes_mode(GF_M2TS_Mux *muxer, GF_M2TS_PackMode au_pes_mode);
GF_Err gf_m2ts_mux_set_initial_pcr(GF_M2TS_Mux *muxer, u64 init_pcr_value);
GF_Err gf_m2ts_mux_enable_pcr_only_packets(GF_M2TS_Mux *muxer, Bool enable_forced_pcr);

void gf_m2ts_mux_program_set_name(GF_M2TS_Mux_Program *program, const char *program_name, const char *mux_provider_name);
void gf_m2ts_mux_enable_sdt(GF_M2TS_Mux *mux, u32 refresh_rate_ms);

void gf_m2ts_flush_pes(GF_M2TS_Demuxer *ts, GF_M2TS_PES *pes);

#endif /*GPAC_DISABLE_MPEG2TS_MUX*/


#ifndef GPAC_DISABLE_MPEG2TS

#endif /*GPAC_DISABLE_MPEG2TS*/

/*! @} */


#ifdef __cplusplus
}
#endif


#endif	//_GF_MPEG_TS_H_
