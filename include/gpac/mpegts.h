/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre, Cyril Concolato, Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2006-2023
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
\file <gpac/mpegts.h>
\brief MPEG-TS demultiplexer and multiplexer APIs
*/

/*!
\addtogroup m2ts_grp MPEG-2 TS
\ingroup media_grp
\brief MPEG-TS demultiplexer and multiplexer APIs.

This section documents the MPEG-TS demultiplexer and multiplexer APIs.

@{
*/


#include <gpac/list.h>
#include <gpac/network.h>
#include <gpac/thread.h>
#include <gpac/internal/odf_dev.h>


#ifdef __cplusplus
extern "C" {
#endif

/*! metadata types for GF_M2TS_METADATA_POINTER_DESCRIPTOR*/
enum {
	GF_M2TS_META_ID3 	= GF_4CC('I','D','3',' '),
};



/*! MPEG-2 Descriptor tags*/
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

	GF_M2TS_HEVC_VIDEO_DESCRIPTOR			= 0x38,

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

	GF_M2TS_DOLBY_VISION_DESCRIPTOR = 0xB0
};

/*! Reserved PID values */
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

/*! max size includes first header, second header, payload and CRC */
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

/*! MPEG-2 TS Media types*/
typedef enum
{
	GF_M2TS_VIDEO_MPEG1						= 0x01,
	GF_M2TS_VIDEO_MPEG2						= 0x02,
	GF_M2TS_AUDIO_MPEG1						= 0x03,
	GF_M2TS_AUDIO_MPEG2						= 0x04,
	GF_M2TS_PRIVATE_SECTION					= 0x05,
	GF_M2TS_PRIVATE_DATA					= 0x06,
	GF_M2TS_MHEG							= 0x07,
	GF_M2TS_13818_1_DSMCC					= 0x08,
	GF_M2TS_H222_1							= 0x09,
	GF_M2TS_13818_6_ANNEX_A					= 0x0A,
	GF_M2TS_13818_6_ANNEX_B					= 0x0B,
	GF_M2TS_13818_6_ANNEX_C					= 0x0C,
	GF_M2TS_13818_6_ANNEX_D					= 0x0D,
	GF_M2TS_13818_1_AUXILIARY				= 0x0E,
	GF_M2TS_AUDIO_AAC						= 0x0F,
	GF_M2TS_VIDEO_MPEG4						= 0x10,
	GF_M2TS_AUDIO_LATM_AAC					= 0x11,
	GF_M2TS_SYSTEMS_MPEG4_PES				= 0x12,
	GF_M2TS_SYSTEMS_MPEG4_SECTIONS			= 0x13,
	GF_M2TS_SYNC_DOWNLOAD_PROTOCOL			= 0x14,
	GF_M2TS_METADATA_PES					= 0x15,
	GF_M2TS_METADATA_SECTION				= 0x16,
	GF_M2TS_METADATA_DATA_CAROUSEL			= 0x17,
	GF_M2TS_METADATA_OBJECT_CAROUSEL		= 0x18,
	GF_M2TS_METADATA_SYNC_DOWNLOAD_PROTOCOL	= 0x19,
	GF_M2TS_IPMP							= 0x1A,
	GF_M2TS_VIDEO_H264						= 0x1B,
	GF_M2TS_MPEG4_AUDIO_NO_SYNTAX			= 0x1C,
	GF_M2TS_MPEG4_TEXT						= 0x1D,
	GF_M2TS_AUX_VIDEO_23002_2				= 0x1E,
	GF_M2TS_VIDEO_SVC						= 0x1F,
	GF_M2TS_VIDEO_MVC						= 0x20,
	GF_M2TS_VIDEO_15444_1					= 0x21,
	GF_M2TS_VIDEO_MPEG2_ADD_STEREO			= 0x22,
	GF_M2TS_VIDEO_H264_ADD_STEREO			= 0x23,
	GF_M2TS_VIDEO_HEVC						= 0x24,
	GF_M2TS_VIDEO_HEVC_TEMPORAL				= 0x25,
	GF_M2TS_VIDEO_MVCD						= 0x26,
	GF_M2TS_TEMI							= 0x27,
	GF_M2TS_VIDEO_SHVC						= 0x28,
	GF_M2TS_VIDEO_SHVC_TEMPORAL				= 0x29,
	GF_M2TS_VIDEO_MHVC						= 0x2A,
	GF_M2TS_VIDEO_MHVC_TEMPORAL				= 0x2B,
	GF_M2TS_GREEN							= 0x2C,
	GF_M2TS_MHAS_MAIN						= 0x2D,
	GF_M2TS_MHAS_AUX						= 0x2E,
	GF_M2TS_QUALITY_SEC 					= 0x2F,
	GF_M2TS_MORE_SEC					 	= 0x30,
	GF_M2TS_VIDEO_HEVC_MCTS					= 0x31,
	GF_M2TS_JPEG_XS							= 0x32,
	GF_M2TS_VIDEO_VVC						= 0x33,
	GF_M2TS_VIDEO_VVC_TEMPORAL				= 0x34,

	GF_M2TS_HLS_AC3_CRYPT		= 0xc1,
	GF_M2TS_HLS_EC3_CRYPT		= 0xc2,
	GF_M2TS_HLS_AAC_CRYPT		= 0xcf,
	GF_M2TS_HLS_AVC_CRYPT		= 0xdb,

	/*the rest is internal use*/

	GF_M2TS_VIDEO_VC1				= 0xEA,
	GF_M2TS_VIDEO_DCII				= 0x80,
	GF_M2TS_AUDIO_AC3				= 0x81,
	GF_M2TS_AUDIO_DTS				= 0x82,
	GF_M2TS_AUDIO_TRUEHD			= 0x83,
	GF_M2TS_AUDIO_EC3				= 0x84,
	GF_M2TS_MPE_SECTIONS            = 0x90,
	GF_M2TS_SUBTITLE_DVB			= 0x100,
	GF_M2TS_AUDIO_OPUS				= 0x101,
	GF_M2TS_VIDEO_AV1				= 0x102,

	GF_M2TS_DVB_TELETEXT			= 0x152,
	GF_M2TS_DVB_VBI					= 0x153,
	GF_M2TS_DVB_SUBTITLE			= 0x154,
	GF_M2TS_METADATA_ID3_HLS		= 0x155,

} GF_M2TSStreamType;


/*! MPEG-2 TS Registration codes types*/
enum
{
	GF_M2TS_RA_STREAM_AC3	= GF_4CC('A','C','-','3'),
	GF_M2TS_RA_STREAM_EAC3	= GF_4CC('E','A','C','3'),
	GF_M2TS_RA_STREAM_VC1	= GF_4CC('V','C','-','1'),
	GF_M2TS_RA_STREAM_HEVC	= GF_4CC('H','E','V','C'),
	GF_M2TS_RA_STREAM_DTS1	= GF_4CC('D','T','S','1'),
	GF_M2TS_RA_STREAM_DTS2	= GF_4CC('D','T','S','2'),
	GF_M2TS_RA_STREAM_DTS3	= GF_4CC('D','T','S','3'),
	GF_M2TS_RA_STREAM_OPUS	= GF_4CC('O','p','u','s'),
	GF_M2TS_RA_STREAM_DOVI	= GF_4CC('D','O','V','I'),
	GF_M2TS_RA_STREAM_AV1	= GF_4CC('A','V','0','1'),

	GF_M2TS_RA_STREAM_GPAC	= GF_4CC('G','P','A','C')
};


/*! MPEG-2 Descriptor tags*/
enum
{
	GF_M2TS_AFDESC_TIMELINE_DESCRIPTOR	= 0x04,
	GF_M2TS_AFDESC_LOCATION_DESCRIPTOR	= 0x05,
	GF_M2TS_AFDESC_BASEURL_DESCRIPTOR	= 0x06,
};

/*! header till the last bit of the section_length field */
#define SECTION_HEADER_LENGTH 3
/*! header from the last bit of the section_length field to the payload */
#define SECTION_ADDITIONAL_HEADER_LENGTH 5
/*! CRC32 length*/
#define	CRC_LENGTH 4



#ifndef GPAC_DISABLE_MPEG2TS
/*! MPEG-2 TS demuxer*/
typedef struct tag_m2ts_demux GF_M2TS_Demuxer;
/*! MPEG-2 TS demuxer elementary stream*/
typedef struct tag_m2ts_es GF_M2TS_ES;
/*! MPEG-2 TS demuxer elementary stream section*/
typedef struct tag_m2ts_section_es GF_M2TS_SECTION_ES;

/*! Maximum number of streams in a TS*/
#define GF_M2TS_MAX_STREAMS	8192

/*! Maximum number of service in a TS*/
#define GF_M2TS_MAX_SERVICES	65535

/*! Maximum size of the buffer in UDP */
#ifdef WIN32
#define GF_M2TS_UDP_BUFFER_SIZE	0x80000
#else
//fixme - issues on linux and OSX with large stack size
//we need to change default stack size for TS thread
#define GF_M2TS_UDP_BUFFER_SIZE	0x40000
#endif

/*! Maximum PCR value */
#define GF_M2TS_MAX_PCR	2576980377811ULL

/*! gets the stream name for an MPEG-2 stream type
\param streamType the target stream type
\return name of the stream type*/
const char *gf_m2ts_get_stream_name(GF_M2TSStreamType streamType);

/*! probes a file for MPEG-2 TS format
\param fileName name / path of the file to brobe
\return GF_TRUE if file is an MPEG-2 TS */
Bool gf_m2ts_probe_file(const char *fileName);

/*! probes a buffer for MPEG-2 TS format
\param data data buffer to probe
\param size size of buffer to probe
\return GF_TRUE if file is an MPEG-2 TS */
Bool gf_m2ts_probe_data(const u8 *data, u32 size);

/*! restamps a set of TS packets by shifting all timing by the given value
\param buffer data buffer to restamp
\param size size of buffer
\param ts_shift clock shift in 90 kHz
\param is_pes array of GF_M2TS_MAX_STREAMS u8 set to 1 for PES PIDs to be restamped, 0 to stream to left untouched
\return error if any
*/
GF_Err gf_m2ts_restamp(u8 *buffer, u32 size, s64 ts_shift, u8 is_pes[GF_M2TS_MAX_STREAMS]);

/*! PES data framing modes*/
typedef enum
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
} GF_M2TSPesFraming;

/*! PES packet flags*/
enum
{
	/*! PES packet is RAP*/
	GF_M2TS_PES_PCK_RAP = 1,
	/*! PES packet contains an AU start*/
	GF_M2TS_PES_PCK_AU_START = 1<<1,
	/*! visual frame starting in this packet is an I frame or IDR (AVC/H264)*/
	GF_M2TS_PES_PCK_I_FRAME = 1<<2,
	/*! visual frame starting in this packet is a P frame*/
	GF_M2TS_PES_PCK_P_FRAME = 1<<3,
	/*! visual frame starting in this packet is a B frame*/
	GF_M2TS_PES_PCK_B_FRAME = 1<<4,
	/*! Possible PCR discontinuity from this packet on*/
	GF_M2TS_PES_PCK_DISCONTINUITY = 1<<5
};

/*! Events used by the MPEGTS demuxer*/
enum
{
	/*! PAT has been found (service connection) - no associated parameter*/
	GF_M2TS_EVT_PAT_FOUND = 0,
	/*! PAT has been updated - no associated parameter*/
	GF_M2TS_EVT_PAT_UPDATE,
	/*! repeated PAT has been found (carousel) - no associated parameter*/
	GF_M2TS_EVT_PAT_REPEAT,
	/*! PMT has been found (service tune-in) - associated parameter: new PMT*/
	GF_M2TS_EVT_PMT_FOUND,
	/*! repeated PMT has been found (carousel) - associated parameter: updated PMT*/
	GF_M2TS_EVT_PMT_REPEAT,
	/*! PMT has been changed - associated parameter: updated PMT*/
	GF_M2TS_EVT_PMT_UPDATE,
	/*! SDT has been received - associated parameter: none*/
	GF_M2TS_EVT_SDT_FOUND,
	/*! repeated SDT has been found (carousel) - associated parameter: none*/
	GF_M2TS_EVT_SDT_REPEAT,
	/*! SDT has been received - associated parameter: none*/
	GF_M2TS_EVT_SDT_UPDATE,
	/*! INT has been received - associated parameter: none*/
	GF_M2TS_EVT_INT_FOUND,
	/*! repeated INT has been found (carousel) - associated parameter: none*/
	GF_M2TS_EVT_INT_REPEAT,
	/*! INT has been received - associated parameter: none*/
	GF_M2TS_EVT_INT_UPDATE,
	/*! PES packet has been received - associated parameter: PES packet*/
	GF_M2TS_EVT_PES_PCK,
	/*! PCR has been received - associated parameter: PES packet with no data*/
	GF_M2TS_EVT_PES_PCR,
	/*! PTS/DTS/PCR info - associated parameter: PES packet with no data*/
	GF_M2TS_EVT_PES_TIMING,
	/*! An MPEG-4 SL Packet has been received in a section - associated parameter: SL packet */
	GF_M2TS_EVT_SL_PCK,
	/*! An IP datagram has been received in a section - associated parameter: IP datagram */
	GF_M2TS_EVT_IP_DATAGRAM,
	/*! Duration has been estimated - associated parameter: PES packet with no data, PTS is duration in msec*/
	GF_M2TS_EVT_DURATION_ESTIMATED,
	/*! TS packet processed - associated parameter: pointer to a GF_M2TS_TSPCK structure*/
	GF_M2TS_EVT_PCK,

	/*! AAC config has been extracted - associated parameter: PES Packet with encoded M4ADecSpecInfo in its data
		THIS MUST BE CLEANED UP
	*/
	GF_M2TS_EVT_AAC_CFG,
#if 0
	/*! An EIT message for the present or following event on this TS has been received */
	GF_M2TS_EVT_EIT_ACTUAL_PF,
	/*! An EIT message for the schedule of this TS has been received */
	GF_M2TS_EVT_EIT_ACTUAL_SCHEDULE,
	/*! An EIT message for the present or following event of an other TS has been received */
	GF_M2TS_EVT_EIT_OTHER_PF,
	/*! An EIT message for the schedule of an other TS has been received */
	GF_M2TS_EVT_EIT_OTHER_SCHEDULE,
#endif
	/*! A message to inform about the current date and time in the TS */
	GF_M2TS_EVT_TDT,
	/*! A message to inform about the current time offset in the TS */
	GF_M2TS_EVT_TOT,
	/*! A generic event message for EIT, TDT, TOT etc */
	GF_M2TS_EVT_DVB_GENERAL,
	/*! MPE / MPE-FEC frame extraction and IP datagrams decryptation */
	GF_M2TS_EVT_DVB_MPE,
	/*! CAT has been found (service tune-in) - associated parameter: new CAT*/
	GF_M2TS_EVT_CAT_FOUND,
	/*! repeated CAT has been found (carousel) - associated parameter: updated CAT*/
	GF_M2TS_EVT_CAT_REPEAT,
	/*! CAT has been changed - associated parameter: updated PMT*/
	GF_M2TS_EVT_CAT_UPDATE,
	/*! AIT has been found (carousel) */
	GF_M2TS_EVT_AIT_FOUND,
	/*! DSCM-CC has been found (carousel) */
	GF_M2TS_EVT_DSMCC_FOUND,

	/*! a TEMI locator has been found or repeated*/
	GF_M2TS_EVT_TEMI_LOCATION,
	/*! a TEMI timecode has been found*/
	GF_M2TS_EVT_TEMI_TIMECODE,
	/*! a stream is about to be removed -  - associated parameter: pointer to GF_M2TS_ES being removed*/
	GF_M2TS_EVT_STREAM_REMOVED
};

/*! table parsing state*/
enum
{
	/*! flag set for start of the table */
	GF_M2TS_TABLE_START		= 1,
	/*! flag set for end of the table */
	GF_M2TS_TABLE_END		= 1<<1,
	/*! flag set when first occurence of the table */
	GF_M2TS_TABLE_FOUND		= 1<<2,
	/*! flag set when update of the table */
	GF_M2TS_TABLE_UPDATE	= 1<<3,
	/*! flag set when repetition of the table - both update and repeat flags may be set if data has changed*/
	GF_M2TS_TABLE_REPEAT	= 1<<4,
};

/*! section callback function
\param demux the target MPEG-2 TS demultiplexer
\param es the target section stream
\param sections the list of gathered sections
\param table_id the ID of the table
\param ex_table_id the extended ID of the table
\param version_number the version number of the table
\param last_section_number the last section number of the table for fragmented cases
\param status the parsing status flags
*/
typedef void (*gf_m2ts_section_callback)(GF_M2TS_Demuxer *demux, GF_M2TS_SECTION_ES *es, GF_List *sections, u8 table_id, u16 ex_table_id, u8 version_number, u8 last_section_number, u32 status);

/*! MPEG-2 TS demuxer section*/
typedef struct __m2ts_demux_section
{
	/*! section data*/
	u8 *data;
	/*! section data size in bytes*/
	u32 data_size;
} GF_M2TS_Section;

/*! MPEG-2 TS demuxer table*/
typedef struct __m2ts_demux_table
{
	/*! pointer to next table/section*/
	struct __m2ts_demux_table *next;
	/*! set when first table completely received*/
	u8 is_init;
	/*! set when repeated section*/
	u8 is_repeat;
	/*! table id*/
	u8 table_id;
	/*! extended table id*/
	u16 ex_table_id;
	/*! current section version number*/
	u8 version_number;
	/*! last received version number*/
	u8 last_version_number;
	/*! current/next indicator (cf MPEG-2 TS spec)*/
	u8 current_next_indicator;
	/*! current section number*/
	u8 section_number;
	/*! last section number to get the complete table*/
	u8 last_section_number;
	/*! list of sections for this table*/
	GF_List *sections;
	/*! total table size*/
	u32 table_size;
} GF_M2TS_Table;


/*! MPEG-2 TS demuxer section filter*/
typedef struct GF_M2TS_SectionFilter
{
	/*! section reassembler*/
	s16 cc;
	/*! section buffer (max 4096)*/
	u8 *section;
	/*! current section length as indicated in section header*/
	u16 length;
	/*! number of bytes received from current section*/
	u16 received;
	/*! section->table aggregator*/
	GF_M2TS_Table *table;
	/*! indicates that the section and last_section_number do not need to be checked */
	Bool process_individual;
	/*! indicates that the section header with table id and extended table id ... is
	   not parsed by the TS demuxer and left for the application  */
	Bool direct_dispatch;
	/*! this field is used for AIT sections, to link the AIT with the program */
	u32 service_id;
	/*! section callback*/
	gf_m2ts_section_callback process_section;
	/*! flag indicatin the demultiplexer has been restarted*/
	Bool demux_restarted;
} GF_M2TS_SectionFilter;

/*! metadata carriage types*/
enum metadata_carriage {
	METADATA_CARRIAGE_SAME_TS		= 0,
	METADATA_CARRIAGE_DIFFERENT_TS	= 1,
	METADATA_CARRIAGE_PS			= 2,
	METADATA_CARRIAGE_OTHER			= 3
};

/*! MPEG-2 TS demuxer metadat pointer*/
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

/*! MPEG-2 TS demuxer TEMI location*/
typedef struct
{
	u32 pid;
	u32 timeline_id;
	//for now we only support one URL announcement
	const char *external_URL;
	Bool is_announce, is_splicing;
	Bool reload_external;
	GF_Fraction activation_countdown;
} GF_M2TS_TemiLocationDescriptor;

/*! MPEG-2 TS demuxer TEMI timecode*/
typedef struct
{
	u32 pid;
	u32 timeline_id;
	u32 media_timescale;
	u64 media_timestamp;
	u64 pes_pts;
	Bool force_reload;
	Bool is_paused;
	Bool is_discontinuity;
	u64 ntp;
} GF_M2TS_TemiTimecodeDescriptor;


/*! MPEG-2 TS program object*/
typedef struct
{
	/*! parent demuxer*/
	GF_M2TS_Demuxer *ts;
	/*! list of streams (PES and sections)*/
	GF_List *streams;
	/*! PID of PMT*/
	u32 pmt_pid;
	/*! PID of PCR*/
	u32 pcr_pid;
	/*! program number*/
	u32 number;
	/*! MPEG-4 IOD if any*/
	GF_InitialObjectDescriptor *pmt_iod;
	/*! list of additional ODs found per program
		this list is only created when MPEG-4 over MPEG-2 is detected
		the list AND the ODs contained in it are destroyed when destroying the program/demuxer
	*/
	GF_List *additional_ods;
	/*! first dts found on this program - this is used by parsers, but not setup by the lib*/
	u64 first_dts;
	/*! Last PCR value received for this program and associated packet number */
	u64 last_pcr_value;
	/*! packet number of last PCR value received*/
	u32 last_pcr_value_pck_number;
	/*! PCR value before the last received one for this program and associated packet number
	used to compute PCR interpolation value*/
	u64 before_last_pcr_value;
	/*! packet number of before last PCR value received*/
	u32 before_last_pcr_value_pck_number;
	/*! for hybrid use-cases we need to know if TDT has already been processed*/
	Bool tdt_found;
	/*! indicates if PID is playing. Used in scalable streams only to toggle quality switch*/
	u32 pid_playing;
	/*! */
	Bool is_scalable;
	/*! metadata descriptor pointer*/
	GF_M2TS_MetadataPointerDescriptor *metadata_pointer_descriptor;
	/*! continuity counter check for pure PCR PIDs*/
	s16 pcr_cc;

	void *user;
} GF_M2TS_Program;

/*! ES flags*/
enum
{
	/*! ES is a PES stream*/
	GF_M2TS_ES_IS_PES = 1,
	/*! ES is a section stream*/
	GF_M2TS_ES_IS_SECTION = 1<<1,
	/*! ES is an mpeg-4 flexmux stream*/
	GF_M2TS_ES_IS_FMC = 1<<2,
	/*! ES is an mpeg-4 SL-packetized stream*/
	GF_M2TS_ES_IS_SL = 1<<3,
	/*! ES is an mpeg-4 Object Descriptor SL-packetized stream*/
	GF_M2TS_ES_IS_MPEG4_OD = 1<<4,
	/*! ES is a DVB MPE stream*/
	GF_M2TS_ES_IS_MPE = 1<<5,
	/*! stream is used to send PCR to upper layer*/
	GF_M2TS_INHERIT_PCR = 1<<6,
	/*! signals the stream is used to send the PCR, but is not the original PID carrying it*/
	GF_M2TS_FAKE_PCR = 1<<7,
	/*! signals the stream type is a gpac codec id*/
	GF_M2TS_GPAC_CODEC_ID = 1<<8,
	/*! signals the stream is a PMT*/
	GF_M2TS_ES_IS_PMT = 1<<9,
	/*! signals the stream is reused (fake pcr streams), i.e. present twice in ess[] list*/
	GF_M2TS_ES_IS_PCR_REUSE = 1<<10,

	/*! all flags above this mask are used by demultiplexer users*/
	GF_M2TS_ES_STATIC_FLAGS_MASK = 0x0000FFFF,

	/*! always send sections regardless of their version_number*/
	GF_M2TS_ES_SEND_REPEATED_SECTIONS = 1<<16,
	/*! flag used to signal next discontinuity on stream should be ignored*/
	GF_M2TS_ES_IGNORE_NEXT_DISCONTINUITY = 1<<17,
	/*! flag used by importers/readers to mark streams that have been seen already in PMT process (update/found)*/
	GF_M2TS_ES_ALREADY_DECLARED = 1<<18,
	/*! flag indicates TEMI info is declared on this stream*/
	GF_M2TS_ES_TEMI_INFO = 1<<19,
	/*! flag indicates each PES is a full AU*/
	GF_M2TS_ES_FULL_AU = 1<<20,
	/*! flag indicates ES is not sparse (AV), used to check discontinuity - set by user*/
	GF_M2TS_CHECK_DISC = 1<<21,
	/*! flag indicates VC1 sequence header is to be checked - set by user*/
	GF_M2TS_CHECK_VC1 = 1<<22,
};

/*! macro for abstract Section/PES stream object, only used for type casting*/
#define ABSTRACT_ES		\
			GF_M2TS_Program *program; \
			u32 flags; \
			u32 pid; \
			u32 stream_type; \
			u32 mpeg4_es_id; \
			GF_SLConfig *slcfg; \
			s16 component_tag; \
			void *user; \
			GF_List *props; \
			u64 first_dts; \
			Bool is_seg_start; \
			u32 service_id;

/*! abstract Section/PES stream object*/
struct tag_m2ts_es
{
	ABSTRACT_ES
};


/*! MPEG-2 TS muxer PES header*/
typedef struct
{
	/*! stream ID*/
	u8 id;
	/*! packet len, 0 if unknown*/
	u16 pck_len;
	/*! data alignment flag*/
	u8 data_alignment;
	/*! packet PTS in 90khz*/
	u64 PTS;
	/*! packet DTS in 90khz*/
	u64 DTS;
	/*! size of PES header*/
	u8 hdr_data_len;
} GF_M2TS_PESHeader;

/*! Section elementary stream*/
struct tag_m2ts_section_es
{
	/*! derive from base M2TS stream*/
	ABSTRACT_ES
	/*! section reassembler*/
	GF_M2TS_SectionFilter *sec;
};

/*! MPEG-2 TS muxer DVB subtitling descriptor*/
typedef struct tag_m2ts_dvb_sub
{
	char language[3];
	u8 type;
	u16 composition_page_id;
	u16 ancillary_page_id;
} GF_M2TS_DVB_Subtitling_Descriptor;

/*! MPEG-2 TS muxer DVB teletext descriptor*/
typedef struct tag_m2ts_dvb_teletext
{
	char language[3];
	u8 type;
	u8 magazine_number;
	u8 page_number;
} GF_M2TS_DVB_Teletext_Descriptor;

/*! MPEG-2 TS muxer metadata descriptor*/
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

//! @cond Doxygen_Suppress

/*! MPEG-2 TS ES object*/
typedef struct tag_m2ts_pes
{
	/*! derive from base M2TS stream*/
	ABSTRACT_ES
	/*! continuity counter check*/
	s16 cc;
	/*! language tag*/
	u32 lang;
	/*! PID of stream this stream depends on*/
	u32 depends_on_pid;

	/*mpegts lib private - do not touch*/
	/*! PES re-assembler data*/
	u8 *pck_data;
	/*! amount of bytes allocated for data */
	u32 pck_alloc_len;
	/*! amount of bytes received in the current PES packet (NOT INCLUDING ANY PENDING BYTES)*/
	u32 pck_data_len;
	/*! size of the PES packet being received, as indicated in pes header length field - can be 0 if unknown*/
	u32 pes_len;
	/*! RAP flag*/
	Bool rap;
	/*! PES PTS in 90khz*/
	u64 PTS;
	/*! PES DTS in 90khz*/
	u64 DTS;
	/*! bytes not consumed from previous PES - shall be less than 9*/
	u8 *prev_data;
	/*! number of bytes not consumed from previous PES - shall be less than 9*/
	u32 prev_data_len;
	/*! number of TS packet containing the start of the current PES*/
	u32 pes_start_packet_number;
	/*! number of TS packet containing the end of the current PES*/
	u32 pes_end_packet_number;
	/*! Last PCR value received for this program*/
	u64 last_pcr_value;
	/*! packet number of last PCR*/
	u32 last_pcr_value_pck_number;
	/*! PCR value before the last received one for this program (used to compute PCR interpolation value)*/
	u64 before_last_pcr_value;
	/*! packet number of before last PCR*/
	u32 before_last_pcr_value_pck_number;
	/*! packet number of preceeding PAT before the PES header*/
	u32 last_pat_packet_number, before_last_pat_pn, before_last_pes_start_pn;

	/*! PES reframer callback. If NULL, pes processing is skipped

	returns the number of bytes NOT consumed from the input data buffer - these bytes are kept when reassembling the next PES packet*/
	u32 (*reframe)(struct tag_m2ts_demux *ts, struct tag_m2ts_pes *pes, Bool same_pts, u8 *data, u32 data_len, GF_M2TS_PESHeader *hdr);

	/*! DVB subtitling info*/
	GF_M2TS_DVB_Subtitling_Descriptor sub;
	/*! Metadata descriptor (for ID3)*/
	GF_M2TS_MetadataDescriptor *metadata_descriptor;

	/*! last received TEMI payload*/
	u8 *temi_tc_desc;
	/*! last received TEMI payload size*/
	u32 temi_tc_desc_len;
	/*! allocated size of TEMI reception buffer*/
	u32 temi_tc_desc_alloc_size;

	/*! last decoded temi (may be one ahead of time as the last received TEMI)*/
	GF_M2TS_TemiTimecodeDescriptor temi_tc;
	/*! flag set to indicate a TEMI descriptor should be flushed with next packet*/
	Bool temi_pending;
	/*! flag set to indicate the last PES packet was not flushed (HLS) to avoid warning on same PTS/DTS used*/
	Bool is_resume;
	/*! DolbiVison info, last byte set to 1 if non-compatible signaling*/
	u8 dv_info[25];

	u64 map_utc, map_utc_pcr, map_pcr;
	u8 *gpac_meta_dsi;
	u32 gpac_meta_dsi_size;
} GF_M2TS_PES;

/*! reserved streamID for PES headers*/
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

/*! SDT (service description table) information*/
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

/*! NIT (network information table) information*/
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

/*! TDT/TOT (Time and Date table) information*/
typedef struct
{
	/*! year*/
	u16 year;
	/*! month, from 0 to 11*/
	u8 month;
	/*! day, from 1 to 31*/
	u8 day;
	/*! hour*/
	u8 hour;
	/*! minute*/
	u8 minute;
	/*! second*/
	u8 second;
} GF_M2TS_TDT_TOT;

/*! DVB Content Description information*/
typedef struct {
	u8 content_nibble_level_1, content_nibble_level_2, user_nibble;
} GF_M2TS_DVB_Content_Descriptor;

/*! DVB Rating information*/
typedef struct {
	char country_code[3];
	u8 value;
} GF_M2TS_DVB_Rating_Descriptor;

/*! DVB Short Event information*/
typedef struct {
	u8 lang[3];
	char *event_name, *event_text;
} GF_M2TS_DVB_Short_Event_Descriptor;

/*! DVB Extended Event entry*/
typedef struct {
	char *item;
	char *description;
} GF_M2TS_DVB_Extended_Event_Item;

/*! DVB Extended Event information*/
typedef struct {
	u8 lang[3];
	u32 last;
	GF_List *items;
	char *text;
} GF_M2TS_DVB_Extended_Event_Descriptor;

/*! DVB EIT Date and Time information*/
typedef struct
{
	time_t unix_time;

	/* local time offset descriptor data (unused ...) */
	u8 country_code[3];
	u8 country_region_id;
	s32 local_time_offset_seconds;
	time_t unix_next_toc;
	s32 next_time_offset_seconds;

} GF_M2TS_DateTime_Event;

/*! DVB EIT component information*/
typedef struct {
	u8 stream_content;
	u8 component_type;
	u8 component_tag;
	u8 language_code[3];
	char *text;
} GF_M2TS_Component;

/*! DVB EIT Event information*/
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

/*! DVB EIT information*/
typedef struct
{
	u32 original_network_id;
	u32 transport_stream_id;
	u16 service_id;
	GF_List *events;
	u8 table_id;
} GF_M2TS_EIT;

/*! MPEG-2 TS packet*/
typedef struct
{
	u8 *data;
	u32 data_len;
	u32 flags;
	u64 PTS, DTS;
	/*parent stream*/
	GF_M2TS_PES *stream;
} GF_M2TS_PES_PCK;

/*! MPEG-4 SL packet from MPEG-2 TS*/
typedef struct
{
	u8 *data;
	u32 data_len;
	u8 version_number;
	/*parent stream */
	GF_M2TS_ES *stream;
} GF_M2TS_SL_PCK;

/*! MPEG-4 SL packet from MPEG-2 TS*/
typedef struct
{
	/*packet start (first byte is TS sync marker)*/
	u8 *data;
	/*packet PID*/
	u32 pid;
	/*parent stream if any/already declared*/
	GF_M2TS_ES *stream;
} GF_M2TS_TSPCK;

typedef struct
{
	u8 version_number;
	u8 table_id;
	u16 ex_table_id;
} GF_M2TS_SectionInfo;

/*! MPEG-2 TS demuxer*/
struct tag_m2ts_demux
{
	/*! PIDs*/
	GF_M2TS_ES *ess[GF_M2TS_MAX_STREAMS];
	/*! programs*/
	GF_List *programs;
	/*! number of PMT received*/
	u32 nb_prog_pmt_received;
	/*! set to GF_TRUE if all PMT received*/
	Bool all_prog_pmt_received;
	/*! set to GF_TRUE if all programs are setup*/
	Bool all_prog_processed;
	/*! received SDTs - keep it separate for now*/
	GF_List *SDTs;
 	/*! UTC time from both TDT and TOT (we currently ignore local offset)*/
	GF_M2TS_TDT_TOT *TDT_time;

	/*! user callback - MUST NOT BE NULL*/
	void (*on_event)(struct tag_m2ts_demux *ts, u32 evt_type, void *par);
	/*! private user data*/
	void *user;

	/*! private resync buffer*/
	u8 *buffer;
	/*! private resync size*/
	u32 buffer_size;
	/*! private alloc size*/
	u32 alloc_size;
	/*! PAT filter*/
	GF_M2TS_SectionFilter *pat;
	/*! CAT filter*/
	GF_M2TS_SectionFilter *cat;
	/*! NIT filter*/
	GF_M2TS_SectionFilter *nit;
	/*! SDT filter*/
	GF_M2TS_SectionFilter *sdt;
	/*! EIT filter*/
	GF_M2TS_SectionFilter *eit;
	/*! TDT/TOT filter*/
	GF_M2TS_SectionFilter *tdt_tot;
	/*! MPEG-4 over 2 is present*/
	Bool has_4on2;
	/*! analyser output */
	FILE *pes_out;

	/*! when set, only pmt and PAT are parsed*/
	Bool seek_mode;
	/*! 192 packet mode with prefix*/
	Bool prefix_present;
	/*! MPE direct flag*/
	Bool direct_mpe;
	/*! DVB-H demu enabled*/
	Bool dvb_h_demux;
	/*! sends packet for at PES header start*/
	Bool notify_pes_timing;

	/*! user callback - MUST NOT BE NULL*/
	void (*on_mpe_event)(struct tag_m2ts_demux *ts, u32 evt_type, void *par);
	/*! structure to hold all the INT tables if the TS contains IP streams */
	struct __gf_dvb_mpe_ip_platform *ip_platform;
	/*! current TS packet number*/
	u32 pck_number;

	/*! TS packet number of last seen packet containing PAT start */
	u32 last_pat_start_num;
	/*! remote file handling - created and destroyed by user*/
	struct __gf_download_session *dnload;
	/*! channel config path*/
	const char *dvb_channels_conf_path;

	/*! AIT*/
	GF_List* ChannelAppList;

	/*! carousel enabled*/
	Bool process_dmscc;
	/*! carousel root dir*/
	char* dsmcc_root_dir;
	/*! DSM-CC objects*/
	GF_List* dsmcc_controler;
	/*! triggers all table reset*/
	Bool table_reset;

	/*! raw mode only parses PAT and PMT, and forward each packet as a GF_M2TS_EVT_PCK event, except PAT packets which must be recreated
	if set, on_event shall be non-null
	*/
	Bool split_mode;
};

//! @endcond

/*! creates a new MPEG-2 TS demultiplexer
\return a new demultiplexer*/
GF_M2TS_Demuxer *gf_m2ts_demux_new();
/*! destroys a new MPEG-2 TS demultiplexer
\param demux the target MPEG-2 TS demultiplexer
*/
void gf_m2ts_demux_del(GF_M2TS_Demuxer *demux);

/*! resets all parsers (PES, sections) of the demultiplexer and trash any pending data in the demux input
\param demux the target MPEG-2 TS demultiplexer
*/
void gf_m2ts_reset_parsers(GF_M2TS_Demuxer *demux);

/*! set all streams is_seg_start variable to GF_TRUE
\param demux the target MPEG-2 TS demultiplexer
*/
void gf_m2ts_mark_seg_start(GF_M2TS_Demuxer *demux);

/*! resets all parsers (PES, sections) of a given program
\param demux the target MPEG-2 TS demultiplexer
\param program the target MPEG-2 TS program
*/
void gf_m2ts_reset_parsers_for_program(GF_M2TS_Demuxer *demux, GF_M2TS_Program *program);
/*! sets PES framing mode of a stream
\param pes the target MPEG-2 TS stream
\param mode the desired PES framing mode
\return error if any
*/
GF_Err gf_m2ts_set_pes_framing(GF_M2TS_PES *pes, GF_M2TSPesFraming mode);

/*! processes input buffer. This will resync the input data to a TS packet start if needed
\param demux the target MPEG-2 TS demultiplexer
\param data the data to process
\param data_size size of the date to process
\return error if any
*/
GF_Err gf_m2ts_process_data(GF_M2TS_Demuxer *demux, u8 *data, u32 data_size);

/*! initializes DSM-CC object carousel reception
\param demux the target MPEG-2 TS demultiplexer
*/
void gf_m2ts_demux_dmscc_init(GF_M2TS_Demuxer *demux);

/*! gets SDT info for a given program
\param demux the target MPEG-2 TS demultiplexer
\param program_id ID of the target MPEG-2 TS program
\return an SDT description, or NULL if not found
*/
GF_M2TS_SDT *gf_m2ts_get_sdt_info(GF_M2TS_Demuxer *demux, u32 program_id);

/*! flushes a given stream. This is used to flush internal demultiplexer buffers on end of stream
\param demux the target MPEG-2 demultiplexer
\param pes the target stream to flush
\param force_flush if >0, flushes all streams, otherwise do not flush stream with known PES length and not yet completed (for HLS)
*/
void gf_m2ts_flush_pes(GF_M2TS_Demuxer *demux, GF_M2TS_PES *pes, u32 force_flush);

/*! flushes all streams in the mux. This is used to flush internal demultiplexer buffers on end of stream
\param demux the target MPEG-2 demultiplexer
\param no_force_flush do not force a flush of incomplete PES (used for HLS)
*/
void gf_m2ts_flush_all(GF_M2TS_Demuxer *demux, Bool no_force_flush);


/*! MPEG-2 TS packet header*/
typedef struct
{
	/*! sync byte 0x47*/
	u8 sync;
	/*! error indicator*/
	u8 error;
	/*! payload start flag (start of PES or section)*/
	u8 payload_start;
	/*! priority flag*/
	u8 priority;
	/*! PID of packet */
	u16 pid;
	/*! scrambling flag ( 0 not scrambled, 1/2 scrambled, 3 reserved)*/
	u8 scrambling_ctrl;
	/*! adaptation field type*/
	u8 adaptation_field;
	/*! continuity counter*/
	u8 continuity_counter;
} GF_M2TS_Header;

/*! MPEG-2 TS packet adaptation field*/
typedef struct
{
	/*! discontunity indicator (for timeline splicing)*/
	u32 discontinuity_indicator;
	/*! random access indicator*/
	u32 random_access_indicator;
	/*! priority indicator*/
	u32 priority_indicator;
	/*! PCR present flag*/
	u32 PCR_flag;
	/*! PCR base value*/
	u64 PCR_base;
	/*! PCR extended value*/
	u64 PCR_ext;
	/*! original PCR flag*/
	u32 OPCR_flag;
	/*! OPCR base value*/
	u64 OPCR_base;
	/*! OPCR extended value*/
	u64 OPCR_ext;
	/*! splicing point flag*/
	u32 splicing_point_flag;
	/*! transport private data flag*/
	u32 transport_private_data_flag;
	/*! AF extension flag*/
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
\addtogroup esi_grp ES Interface
\ingroup media_grp
\brief Basic stream interface API used by MPEG-2 TS muxer.

This section documents the ES interface used by the MPEG-2 TS muxer. This interface is used to
describe streams and packets consumed by the TS muxer independently from the rest of GPAC (filter packets)

@{
*/

/*! ESI input control commands*/
enum
{
	/*! forces a data flush from interface to dest (caller) - used for non-threaded interfaces
		corresponding parameter: unused
	*/
	GF_ESI_INPUT_DATA_FLUSH,
	/*! pulls a COMPLETE AU from the stream
		corresponding parameter: pointer to a GF_ESIPacket to fill. The input data_len in the packet is used to indicate any padding in bytes
	*/
	GF_ESI_INPUT_DATA_PULL,
	/*! releases the currently pulled AU from the stream - AU cannot be pulled after that, unless seek happens
		corresponding parameter: unused
	*/
	GF_ESI_INPUT_DATA_RELEASE,

	/*! destroys any allocated resource by the stream interface*/
	GF_ESI_INPUT_DESTROY,
};

/*! ESI output control commands*/
enum
{
	/*! forces a data flush from interface to dest (caller) - used for non-threaded interfaces
		corresponding parameter: GF_ESIPacket
	*/
	GF_ESI_OUTPUT_DATA_DISPATCH
};

/*! data packet flags*/
enum
{
	/*! packet is an access unit start*/
	GF_ESI_DATA_AU_START	=	1,
	/*! packet is an access unit end*/
	GF_ESI_DATA_AU_END		=	1<<1,
	/*! packet has valid CTS*/
	GF_ESI_DATA_HAS_CTS		=	1<<2,
	/*! packet has valid DTS*/
	GF_ESI_DATA_HAS_DTS		=	1<<3,
	/*! packet contains a carousel repeated AU*/
	GF_ESI_DATA_REPEAT		=	1<<4,
};

/*! stream interface for MPEG-2 TS muxer*/
typedef struct __data_packet_ifce
{
	/*! packet flags*/
	u16 flags;
	/*! SAP type*/
	u8 sap_type;
	/*! payload*/
	u8 *data;
	/*! payload size*/
	u32 data_len;
	/*! DTS expressed in media timescale*/
	u64 dts;
	/*! CTS/PTS expressed in media timescale*/
	u64 cts;
	/*! duration expressed in media timescale*/
	u32 duration;
	/*! packet sequence number*/
	u32 pck_sn;
	/*! MPEG-4 Access Unit sequence number (use for carousel of BIFS/OD AU)*/
	u32 au_sn;
	/*! ISM BSO for for packets using ISMACrypt/OMA/3GPP based crypto*/
	u32 isma_bso;
	/*! serialized list of AF descriptors*/
	u8 *mpeg2_af_descriptors;
	/*! size of serialized list of AF descriptors*/
	u32 mpeg2_af_descriptors_size;
} GF_ESIPacket;

/*! video information for stream interface*/
struct __esi_video_info
{
	/*! video width in pixels*/
	u32 width;
	/*! video height in pixels*/
	u32 height;
	/*! pixel aspect ratio as (num<<16)|den*/
	u32 par;
	/*! video framerate*/
	Double FPS;
};
/*! audio information for stream interface*/
struct __esi_audio_info
{
	/*! sample rate*/
	u32 sample_rate;
	/*! number of channels*/
	u32 nb_channels;
};

/*! ES interface capabilities*/
enum
{
	/*! data can be pulled from this stream*/
	GF_ESI_AU_PULL_CAP	=	1,
	/*! no more data to expect from this stream*/
	GF_ESI_STREAM_IS_OVER	=	1<<2,
	/*! stream is not signaled through MPEG-4 Systems (OD stream) */
	GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS =	1<<3,
	/*! stream is not signaled through MPEG-4 Systems (OD stream) */
	GF_ESI_AAC_USE_LATM =	1<<4,
	/*! temporary end of stream (flush of segment)*/
	GF_ESI_STREAM_FLUSH	=	1<<5,
	/*! stream uses HLS SAES encryption*/
	GF_ESI_STREAM_HLS_SAES	=	1<<6,
	/*! stream uses non-backward DolbyVision signaling*/
	GF_ESI_FORCE_DOLBY_VISION = 1<<7,
	/*! sparse stream with currently no packets*/
	GF_ESI_STREAM_SPARSE = 1<<8,
};

/*! elementary stream information*/
typedef struct __elementary_stream_ifce
{
	/*! misc caps of the stream*/
	u32 caps;
	/*! matches PID for MPEG2, ES_ID for MPEG-4*/
	u32 stream_id;
	/*! MPEG-4 ST*/
	u8 stream_type;
	/*! codec ID*/
	u32 codecid;
	/*! packed 3-char language code (4CC with last byte ' ')*/
	u32 lang;
	/*! media timescale*/
	u32 timescale;
	/*! duration in ms - 0 if unknown*/
	Double duration;
	/*! average bit rate in bit/sec - 0 if unknown*/
	u32 bit_rate;
	/*! repeat rate in ms for carrouseling - 0 if no repeat*/
	u32 repeat_rate;
	/*! decoder configuration*/
	u8 *decoder_config;
	/*! decoder configuration size*/
	u32 decoder_config_size;

	/*! MPEG-4 SL Config if any*/
	GF_SLConfig *sl_config;

	/*! input ES control from caller*/
	GF_Err (*input_ctrl)(struct __elementary_stream_ifce *_self, u32 ctrl_type, void *param);
	/*! input user data of interface - usually set by interface owner*/
	void *input_udta;

	/*! output ES control of destination*/
	GF_Err (*output_ctrl)(struct __elementary_stream_ifce *_self, u32 ctrl_type, void *param);
	/*! output user data of interface - usually set during interface setup*/
	void *output_udta;
	/*! stream dependency ID*/
	u32 depends_on_stream;

	/*! dv info, not valid if first byte not 1*/
	u8 dv_info[24];

	/*! registration authority code to use, 0 if not applicable*/
	u32 ra_code;
	/*! GPAC unmapped meta codec decoder config size*/
	u32 gpac_meta_dsi_size;
	/*! GPAC unmapped meta codec decoder config*/
	u8 *gpac_meta_dsi;
	/*! GPAC unmapped meta codec name if knwon*/
	const char *gpac_meta_name;
} GF_ESInterface;

/*! @} */

/*
	MPEG-2 TS Multiplexer
*/

/*! adaptation field type*/
enum {
	/*! reserved*/
	GF_M2TS_ADAPTATION_RESERVED	= 0,
	/*! no adaptation (payload only)*/
	GF_M2TS_ADAPTATION_NONE		= 1,
	/*! adaptation only (no payload)*/
	GF_M2TS_ADAPTATION_ONLY		= 2,
	/*! adaptation and payload*/
	GF_M2TS_ADAPTATION_AND_PAYLOAD 	= 3,
};

/*! MPEG-2 TS muxer program*/
typedef struct __m2ts_mux_program GF_M2TS_Mux_Program;
/*! MPEG-2 TS muxer*/
typedef struct __m2ts_mux GF_M2TS_Mux;

/*! MPEG-2 TS muxer section*/
typedef struct __m2ts_section {
	/*! pointer to next section*/
	struct __m2ts_section *next;
	/*! section data*/
	u8 *data;
	/*! section size*/
	u32 length;
} GF_M2TS_Mux_Section;

/*! MPEG-2 TS muxer table*/
typedef struct __m2ts_table {
	/*! pointer to next table*/
	struct __m2ts_table *next;
	/*! table ID (we don't handle extended table ID as part of the section muxer*/
	u8 table_id;
	/*! version number of the table*/
	u8 version_number;
	/*! list of sections for the table (one or more)*/
	struct __m2ts_section *section;
} GF_M2TS_Mux_Table;

/*! MPEG-2 TS muxer time information*/
typedef struct
{
	/*! seconds*/
	u32 sec;
	/*! nanoseconds*/
	u32 nanosec;
} GF_M2TS_Time;


/*! MPEG-2 TS muxer packet*/
typedef struct __m2ts_mux_pck
{
	/*! pointer to next packet (we queue packets if needed)*/
	struct __m2ts_mux_pck *next;
	/*! payload*/
	u8 *data;
	/*! payload size in bytes*/
	u32 data_len;
	/*! flags*/
	u16 flags;
	/*! sap type*/
	u8 sap_type;
	/*! CTS in 90 khz*/
	u64 cts;
	/*! DTS in 90 khz*/
	u64 dts;
	/*! duration in 90 khz*/
	u32 duration;
	/*! serialized list of adaptation field descriptors if any*/
	u8 *mpeg2_af_descriptors;
	/*! size of serialized list of adaptation field descriptors*/
	u32 mpeg2_af_descriptors_size;
} GF_M2TS_Packet;

/*! MPEG-2 TS muxer stream*/
typedef struct __m2ts_mux_stream {
	/*! next stream*/
	struct __m2ts_mux_stream *next;
	/*! pid*/
	u32 pid;
	/*! CC of the stream*/
	u8 continuity_counter;
	/*! parent program*/
	struct __m2ts_mux_program *program;
	/*! average stream bit-rate in bit/sec*/
	u32 bit_rate;
	/*! multiplexer time - NOT THE PCR*/
	GF_M2TS_Time time;
	/*! next time for packet*/
	GF_M2TS_Time next_time;
	/*! allow PCR only packets*/
	Bool pcr_only_mode;
	/*! tables for section PIDs*/
	GF_M2TS_Mux_Table *tables;
	/*! total table sizes for bitrate estimation (PMT/PAT/...)*/
	u32 total_table_size;
	/*! current table - used for on-the-fly packetization of sections */
	GF_M2TS_Mux_Table *current_table;
	/*! active section*/
	GF_M2TS_Mux_Section *current_section;
	/*! current section offset*/
	u32 current_section_offset;
	/*! carousel rate in ms*/
	u32 refresh_rate_ms;
	/*! init verision of table*/
	u8 initial_version_number;
	/*! PES version of transport for this codec type is forced*/
	u8 force_pes;
	/*! table needs updating*/
	Bool table_needs_update;
	/*! table needs send*/
	Bool table_needs_send;
	/*! force one AU per PES*/
	Bool force_single_au;
	/*! force initial discontinuity*/
	Bool set_initial_disc;
	/*! force registration descriptor*/
	Bool force_reg_desc;

	/*! minimal amount of bytes we are allowed to copy frome next AU in the current PES. If no more than this
	is available in PES, don't copy from next*/
	u32 min_bytes_copy_from_next;
	/*! process PES or table update/framing
	returns the priority of the stream,  0 meaning not scheduled, 1->N highest priority sent first*/
	u32 (*process)(struct __m2ts_mux *muxer, struct __m2ts_mux_stream *stream);

	/*! stream type*/
	GF_M2TSStreamType mpeg2_stream_type;
	/*! stream ID*/
	u32 mpeg2_stream_id;
	/*! priority*/
	u32 scheduling_priority;

 	/*! current packet being processed - does not belong to the packet fifo*/
	GF_ESIPacket curr_pck;
	/*! offset in packet*/
	u32 pck_offset;
	/*! size of next payload, 0 if unknown*/
	u32 next_payload_size;
	/*! number of bytes to copy from next packet*/
	u32 copy_from_next_packets;
	/*! size of next next payload, 0 if unknown*/
	u32 next_next_payload_size;
	/*! size of next next next payload, 0 if unknown*/
	u32 next_next_next_payload_size;
	/*! size of packetized packet*/
	u32 pes_data_len;
	/*! remaining bytes to send as TS packets*/
	u32 pes_data_remain;
	/*! if set forces a new PES packet at next packet*/
	Bool force_new;
	/*! flag set to indicate packet data shall be freed (rewrite of source packet)*/
	Bool discard_data;
	/*! flags of next packet*/
	u16 next_pck_flags;
	/*! SAP type of next packet*/
	u8 next_pck_sap;
	/*! CTS of next packet*/
	u64 next_pck_cts;
	/*! DTS of next packet*/
	u64 next_pck_dts;
	/*! overhead of reframing in bytes (ex, ADTS header or LATM rewrite)*/
	u32 reframe_overhead;
	/*! if set, forces a PES start at each RAP*/
	Bool start_pes_at_rap;
	/*! if set, do not have more than one AU start in a PES packet*/
	Bool prevent_two_au_start_in_pes;
	/*! ES interface of stream*/
	struct __elementary_stream_ifce *ifce;
	/*! timescale scale factor (90khz / es_ifce.timeScale)*/
	GF_Fraction ts_scale;

	/*! packet fifo head*/
	GF_M2TS_Packet *pck_first;
	/*! packet fifo tail*/
	GF_M2TS_Packet *pck_last;
	/*! packet reassembler (PES packets are most of the time full frames)*/
	GF_M2TS_Packet *pck_reassembler;
	/*! mutex if enabled*/
	GF_Mutex *mx;
	/*! avg bitrate computed*/
	u64 last_br_time;
	/*! number of bytes sent since last bitrate estimation*/
	u32 bytes_since_last_time;
	/*! number of PES packets sent since last bitrate estimation*/
	u32 pes_since_last_time;
	/*! last DTS sent*/
	u64 last_dts;
	/*! MPEG-4 over MPEG-2 sections table id (BIFS, OD, ...)*/
	u8 table_id;
	/*! MPEG-4 SyncLayer header of the packet*/
	GF_SLHeader sl_header;
	/*! last time when the AAC config was sent for LATM*/
	u32 latm_last_aac_time;
	/*! list of GF_M2TSDescriptor to add to the MPEG-2 stream. By default set to NULL*/
	GF_List *loop_descriptors;

	/*! packet SAP type when segmenting the TS*/
	u32 pck_sap_type;
	/*! packet SAP time (=PTS) when segmenting the TS*/
	u64 pck_sap_time;

	/*! last process result*/
	u32 process_res;
} GF_M2TS_Mux_Stream;

/*! MPEG-4 systems signaling mode*/
enum {
	/*! no MPEG-4 signaling*/
	GF_M2TS_MPEG4_SIGNALING_NONE = 0,
	/*! Full MPEG-4 signaling*/
	GF_M2TS_MPEG4_SIGNALING_FULL,
	/*MPEG-4 over MPEG-2 profile where PES media streams may be referred to by the scene without SL-packetization*/
	GF_M2TS_MPEG4_SIGNALING_SCENE
};

/*! MPEG-2 TS base descriptor*/
typedef struct __m2ts_base_descriptor
{
	/*! descriptor tag*/
	u8 tag;
	/*! descriptor size*/
	u8 data_len;
	/*! descriptor payload*/
	u8 *data;
} GF_M2TSDescriptor;

/*! MPEG-2 TS muxer program info */
struct __m2ts_mux_program {
	/*! next program*/
	struct __m2ts_mux_program *next;
	/*! parent muxer*/
	struct __m2ts_mux *mux;
	/*! program number*/
	u16 number;
	/*! all streams but PMT*/
	GF_M2TS_Mux_Stream *streams;
	/*! PMT stream*/
	GF_M2TS_Mux_Stream *pmt;
	/*! pointer to PCR stream*/
	GF_M2TS_Mux_Stream *pcr;
	/*! TS time at pcr init*/
	GF_M2TS_Time ts_time_at_pcr_init;
	/*! pcr init time (rand or fixed by user)*/
	u64 pcr_init_time;
	/*! number of TS packet at PCR init*/
	u64 num_pck_at_pcr_init;
	/*! last PCR value in 27 mhz*/
	u64 last_pcr;
	/*! last DTS value in 90 khz*/
	u64 last_dts;
	/*! system clock in microseconds at last PCR*/
	u64 sys_clock_at_last_pcr;
	/*! number of packets at last PCR*/
	u64 nb_pck_last_pcr;
	/*! min initial DTS of all streams*/
	u64 initial_ts;
	/*! indicates that min initial DTS of all streams is set*/
	u32 initial_ts_set;
	/*! if set, injects a PCR offset*/
	Bool pcr_init_time_set;
	/*! PCR offset to inject*/
	u64 pcr_offset;
	/*! Forced value of first packet CTS*/
	u64 force_first_pts;
	/*! indicates the initial discontinuity flag should be set on first packet of all streams*/
	Bool initial_disc_set;
	/*! MPEG-4 IOD for 4on2*/
	GF_Descriptor *iod;
	/*! list of GF_M2TSDescriptor to add to the program descriptor loop. By default set to NULL, if non null will be reset and destroyed upon cleanup*/
	GF_List *loop_descriptors;
	/*! MPEG-4 signaling type*/
	u32 mpeg4_signaling;
	/*! if set, use MPEG-4 signaling only for BIFS and OD streams*/
	Bool mpeg4_signaling_for_scene_only;
	/*! program name (for SDT)*/
	char *name;
	/*! program provider (for SDT)*/
	char *provider;
	/*! CTS offset in 90khz*/
	u32 cts_offset;
};

/*! AU packing per pes configuration*/
typedef enum
{
	/*! only audio AUs are packed in a single PES, video and systems are not (recommended default)*/
	GF_M2TS_PACK_AUDIO_ONLY=0,
	/*! never pack AUs in a single PES*/
	GF_M2TS_PACK_NONE,
	/*! always try to pack AUs in a single PES*/
	GF_M2TS_PACK_ALL
} GF_M2TS_PackMode;

/*! MPEG-2 TS muxer*/
struct __m2ts_mux {
	/*! list of programs*/
	GF_M2TS_Mux_Program *programs;
	/*! PAT stream*/
	GF_M2TS_Mux_Stream *pat;
	/*! SDT stream*/
	GF_M2TS_Mux_Stream *sdt;
	/*! TS ID*/
	u16 ts_id;
	/*! signals reconfigure is needed (PAT)*/
	Bool needs_reconfig;

	/*! used to indicate that the input data is pushed to the muxer (i.e. not read from a file)
	or that the output data is sent on sockets (not written to a file) */
	Bool real_time;

	/*! indicates if the multiplexer shall target a fix bit rate (monitoring timing and produce padding packets)
	   or if the output stream will contain only input data*/
	Bool fixed_rate;

	/*! output bit-rate in bit/sec*/
	u32 bit_rate;

	/*! init value for PCRs on all streams if 0, random value is used*/
	u64 init_pcr_value;
	/*! PCR update rate in milliseconds*/
	u32 pcr_update_ms;
	/*! destination TS packet*/
	char dst_pck[188];
	/*! destination NULL TS packet*/
	char null_pck[188];

	/*! multiplexer time, incremented each time a packet is sent
	  used to monitor the sending of muxer related data (PAT, ...) */
	GF_M2TS_Time time;

	/*! Time of the muxer when the first call to process is made (first packet sent?) */
	GF_M2TS_Time init_ts_time;

	/*! System time in microsenconds when the muxer is started */
	u64 init_sys_time;
	/*! forces PAT injection at next packet*/
	Bool force_pat;
	/*! AU per PES packing mode*/
	GF_M2TS_PackMode au_pes_mode;
	/*! enable forced PCR (PCR only packets)*/
	Bool enable_forced_pcr;
	/*! last rate estimation time in microseconds*/
	u64 last_br_time_us;
	/*! number of TS packets over the bitrate estimation period*/
	u32 pck_sent_over_br_window;
	/*! number of TS packets sent*/
	u64 tot_pck_sent;
	/*! number of TS NULL packets sent*/
	u64 tot_pad_sent;
	/*! number of PES bytes sent*/
	u64 tot_pes_pad_bytes;

	/*! average rate in kbps*/
	u32 average_birate_kbps;

	/*! if set, flushes all streams in a program when a PES RAP is found (eg sent remaining audios and co)*/
	Bool flush_pes_at_rap;
	/*! state for forced injection of PAT/PMT/PCR*/
	u32 force_pat_pmt_state;

	/*! static write bitstream object for formatting packets*/
	GF_BitStream *pck_bs;
	/*! PID to watch for SAP insertions*/
	u32 ref_pid;
	/* if the packet output starts (first PES) with the first packet of a SAP AU (used when dashing), set to TRUE*/
	Bool sap_inserted;
	/*! SAP time (used when dashing)*/
	u64 sap_time;
	/*! SAP type (used when dashing)*/
	u32 sap_type;
	/*! last PTS in 90khz (used when dashing to build sidx)*/
	u64 last_pts;
	/*! last PID (used when dashing to build sidx)*/
	u32 last_pid;
};

/*! default refresh rate for PSI data*/
#define GF_M2TS_PSI_DEFAULT_REFRESH_RATE	200
/*! creates a new MPEG-2 TS multiplexer
\param mux_rate target mux rate in kbps, can be 0 for VBR
\param pat_refresh_rate PAT refresh rate in ms
\param real_time indicates if real-time production should be used
\return a new MPEG-2 TS multiplexer
 */
GF_M2TS_Mux *gf_m2ts_mux_new(u32 mux_rate, u32 pat_refresh_rate, Bool real_time);
/*! destroys a TS muxer
\param muxer the target MPEG-2 TS multiplexer
*/
void gf_m2ts_mux_del(GF_M2TS_Mux *muxer);

/*! sets max interval between two PCR. Default/max interval is 100 milliseconds
\param muxer the target MPEG-2 TS multiplexer
\param pcr_update_ms PCR interval in milliseconds
*/
void gf_m2ts_mux_set_pcr_max_interval(GF_M2TS_Mux *muxer, u32 pcr_update_ms);
/*! adds a new program to the muxer
\param muxer the target MPEG-2 TS multiplexer
\param program_number number of program to add
\param pmt_pid value of program PMT packet identifier
\param pmt_refresh_rate refresh rate in milliseconds of PMT. 0 only sends PMT once
\param pcr_offset initial offset in 90 kHz to add to PCR values (usually to compensate for large frame sending times)
\param mpeg4_signaling type of MPEG-4 signaling used
\param pmt_version initial version of the PMT
\param initial_disc if GF_TRUE, signals packet discontinuity on the first packet of eact stream in the program
\param force_first_pts if not 0, the first PTS written in the program will have the indicated value (in 90khz)
\return a TS multiplexer program
*/
GF_M2TS_Mux_Program *gf_m2ts_mux_program_add(GF_M2TS_Mux *muxer, u32 program_number, u32 pmt_pid, u32 pmt_refresh_rate, u64 pcr_offset, u32 mpeg4_signaling, u32 pmt_version, Bool initial_disc, u64 force_first_pts);
/*! adds a stream to a program
\param program the target program
\param ifce the stream interface object for packet and properties query
\param pid the packet identifier for the stream
\param is_pcr if GF_TRUE, this stream is the clock reference of the program
\param force_pes_mode if GF_TRUE, forces PES packetization (used for streams carried over PES or sections, such as metadata and MPEG-4)
\param needs_mutex indicates a mutex is required before calling the stream interface object
\return a TS multiplexer program
*/
GF_M2TS_Mux_Stream *gf_m2ts_program_stream_add(GF_M2TS_Mux_Program *program, GF_ESInterface *ifce, u32 pid, Bool is_pcr, Bool force_pes_mode, Bool needs_mutex);
/*! updates muxer configuration
\param muxer the target MPEG-2 TS multiplexer
\param reset_time if GF_TRUE, resets multiplexer time to 0
*/
void gf_m2ts_mux_update_config(GF_M2TS_Mux *muxer, Bool reset_time);
/*! removes stream from program, triggering PMT update as well
\param stream the target stream to remove
*/
void gf_m2ts_program_stream_remove(GF_M2TS_Mux_Stream *stream);

/*! finds a program by its number
\param muxer the target MPEG-2 TS multiplexer
\param program_number number of program to retrieve
\return a TS multiplexer program or NULL of not found
*/
GF_M2TS_Mux_Program *gf_m2ts_mux_program_find(GF_M2TS_Mux *muxer, u32 program_number);
/*! gets number of program in a multiplex
\param muxer the target MPEG-2 TS multiplexer
\return number of programs
*/
u32 gf_m2ts_mux_program_count(GF_M2TS_Mux *muxer);
/*! gets number of stream in a program
\param program the target program
\return number of streams
*/
u32 gf_m2ts_mux_program_get_stream_count(GF_M2TS_Mux_Program *program);
/*! gets the packet identifier of the PMT of a program
\param program the target program
\return PID of the PMT
*/
u32 gf_m2ts_mux_program_get_pmt_pid(GF_M2TS_Mux_Program *program);
/*! gets the packet identifier of the PCR stream of a program
\param program the target program
\return PID of the PCR
*/
u32 gf_m2ts_mux_program_get_pcr_pid(GF_M2TS_Mux_Program *program);

/*! state of multiplexer*/
typedef enum
{
	/*! multiplexer is idle, nothing to produce (real-time mux only)*/
	GF_M2TS_STATE_IDLE,
	/*! multiplexer is sending data*/
	GF_M2TS_STATE_DATA,
	/*! multiplexer is sending padding (no input data and fixed rate required)*/
	GF_M2TS_STATE_PADDING,
	/*! multiplexer is done*/
	GF_M2TS_STATE_EOS,
} GF_M2TSMuxState;

/*! produces one packet of the multiplex
\param muxer the target MPEG-2 TS multiplexer
\param status set to the current state of the multiplexer
\param usec_till_next the target MPEG-2 TS multiplexer
\return packet produced or NULL if error or idle
*/
const u8 *gf_m2ts_mux_process(GF_M2TS_Mux *muxer, GF_M2TSMuxState *status, u32 *usec_till_next);
/*! gets the system clock of the multiplexer (time elapsed since start)
\param muxer the target MPEG-2 TS multiplexer
\return system clock of the multiplexer in milliseconds
*/
u32 gf_m2ts_get_sys_clock(GF_M2TS_Mux *muxer);
/*! gets the multiplexer clock (computed from mux rate and packet sent)
\param muxer the target MPEG-2 TS multiplexer
\return multiplexer clock in milliseconds
*/
u32 gf_m2ts_get_ts_clock(GF_M2TS_Mux *muxer);

/*! gets the multiplexer clock (computed from mux rate and packet sent)
\param muxer the target MPEG-2 TS multiplexer
\return multiplexer clock in 90kHz scale
*/
u64 gf_m2ts_get_ts_clock_90k(GF_M2TS_Mux *muxer);

/*! set AU per PES packetization mode
\param muxer the target MPEG-2 TS multiplexer
\param au_pes_mode AU packetization mode to use
\return error if any
*/
GF_Err gf_m2ts_mux_use_single_au_pes_mode(GF_M2TS_Mux *muxer, GF_M2TS_PackMode au_pes_mode);

/*! sets initial PCR value for all programs in multiplex. If this is not called, each program will use a random initial PCR
\param muxer the target MPEG-2 TS multiplexer
\param init_pcr_value initial PCR value in 90kHz
\return error if any
*/
GF_Err gf_m2ts_mux_set_initial_pcr(GF_M2TS_Mux *muxer, u64 init_pcr_value);
/*! enables pure PCR packets (no payload). This can be needed for some DVB constraints on PCR refresh rate (less than one frame duration) when next video frame is not yet ready to be muxed.

\param muxer the target MPEG-2 TS multiplexer
\param enable_forced_pcr if GF_TRUE, the multiplexer may use PCR-only packets; otherwise, it will wait for a frame to be ready on the PCR stream before sending a PCR
\return error if any
*/
GF_Err gf_m2ts_mux_enable_pcr_only_packets(GF_M2TS_Mux *muxer, Bool enable_forced_pcr);

/*! sets program name and provider
\param program the target program
\param program_name the program name (UTF-8)
\param mux_provider_name the provider name (UTF-8)
*/
void gf_m2ts_mux_program_set_name(GF_M2TS_Mux_Program *program, const char *program_name, const char *mux_provider_name);

/*! forces keeping DTS CTS untouched (no remap of first packet to 0) - needed when doing stateless dashing
\param program the target program
*/
void gf_m2ts_mux_program_force_keep_ts(GF_M2TS_Mux_Program *program);

/*! enables sending DVB SDT
\param muxer the target program
\param refresh_rate_ms the refresh rate for the SDT. A value of 0 only sends the SDT once
*/
void gf_m2ts_mux_enable_sdt(GF_M2TS_Mux *muxer, u32 refresh_rate_ms);

#endif /*GPAC_DISABLE_MPEG2TS_MUX*/


#ifndef GPAC_DISABLE_MPEG2TS

#endif /*GPAC_DISABLE_MPEG2TS*/

/*! @} */


#ifdef __cplusplus
}
#endif


#endif	//_GF_MPEG_TS_H_
