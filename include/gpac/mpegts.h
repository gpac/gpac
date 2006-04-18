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

/*Maximum number of streams in a TS*/
#define GF_M2TS_MAX_STREAMS	8192

/*MPEG-2 TS Media types*/
enum
{
	GF_M2TS_VIDEO_MPEG1 = 0x01,
	GF_M2TS_VIDEO_MPEG2 = 0x02,
	GF_M2TS_AUDIO_MPEG1 = 0x03,
	GF_M2TS_AUDIO_MPEG2 = 0x04, 
	GF_M2TS_PRIVATE_SECTION = 0x05,
	GF_M2TS_PRIVATE_DATA  = 0x06,
	GF_M2TS_AUDIO_AAC = 0x0f,
	GF_M2TS_VIDEO_MPEG4 = 0x10,
	GF_M2TS_VIDEO_H264 = 0x1b,

	GF_M2TS_AUDIO_AC3 = 0x81,
	GF_M2TS_AUDIO_DTS = 0x8a,
	GF_M2TS_SUBTITLE_DVB = 0x100,
};

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
	GF_M2TS_PES_PCK_AU_START = 1<<1
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
	/*PMT has been changed - assoctiated parameter: updated PMT*/
	GF_M2TS_EVT_PMT_UPDATE,
	/*SDT has been received - assoctiated parameter: none*/
	GF_M2TS_EVT_SDT_FOUND,
	/*SDT has been received - assoctiated parameter: none*/
	GF_M2TS_EVT_SDT_UPDATE,
	/*PES packet has been received - assoctiated parameter: PES packet*/
	GF_M2TS_EVT_PES_PCK,
	/*PCR has been received - assoctiated parameter: PES packet with no data*/
	GF_M2TS_EVT_PES_PCR,
};

/*MPEG-2 TS section object (PAT, PMT, etc..)*/
typedef struct
{
	/*set to 1 once the section has been parsed and loaded - used to discard carousel'ed data*/
	u8 section_init;

	/*section reassembler*/
	s16 cc;
	unsigned char *section;
	u16 section_recv;
	/*section header*/
	u16 section_len;
	u8 table_id;
	u8 syntax_indicator;
	u16 sec_id;
	u8 version_number;
	u8 current_next_indicator;
	u8 section_number;
	u8 last_section_number;
	u8 start;
	/*error indiator*/
	u8 had_error;
} GF_M2TS_Section;

/*MPEG-2 TS program object*/
typedef struct 
{
	GF_List *streams;
	u32 pmt_pid;  
	u32 pcr_pid;
	u32 number;
	/*first dts found on this program - this is used by parsers, but not setup by the lib*/
	u64 first_dts;
} GF_M2TS_Program;

/*Abstract Section/PES stream object, only used for type casting*/
typedef struct 
{
	GF_M2TS_Program *program;
	u32 pid;
} GF_M2TS_ES;

/*MPEG-2 TS ES object*/
typedef struct tag_m2ts_pes
{
	GF_M2TS_Program *program;
	u32 pid;
	u8 stream_type;
	u32 lang;

	/*object info*/
	u32 vid_w, vid_h, vid_par, aud_sr, aud_nb_ch;
	/*user private*/
	void *user;


	/*mpegts lib private - do not touch :)*/
	/*PES re-assembler*/
	unsigned char *data;
	u32 data_len;
	Bool rap;
	u64 PTS, DTS;
	
	/*PES reframer - if NULL, pes processing is skiped*/
	u32 frame_state;
	void (*reframe)(struct tag_m2ts_demux *ts, struct tag_m2ts_pes *pes, u64 DTS, u64 CTS, unsigned char *data, u32 data_len);
} GF_M2TS_PES;


/*MPEG-2 TS PMT stream*/
typedef struct 
{
	GF_M2TS_Program *program;
	u32 pid;
	/*section filter*/
	GF_M2TS_Section *sec;
} GF_M2TS_PMT;

/*SDT information object*/
typedef struct
{
	u32 service_id;
	u32 EIT_schedule;
	u32 EIT_present_following;
	u32 running_status;
	u32 free_CA_mode;
	u8 service_type;
	char *provider, *service;
} GF_M2TS_SDT;

/*MPEG-2 TS packet*/
typedef struct
{
	unsigned char *data;
	u32 data_len;
	u32 flags;
	u64 PTS, DTS;
	/*parent stream*/
	GF_M2TS_PES *stream;
} GF_M2TS_PES_PCK;

/*MPEG-2 TS demuxer*/
typedef struct tag_m2ts_demux
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
	unsigned char *buffer;
	u32 buffer_size, alloc_size;
	/*default transport PID filters*/
	GF_M2TS_Section *pas, *nit, *sdt;
} GF_M2TS_Demuxer;


GF_M2TS_Demuxer *gf_m2ts_demux_new();
void gf_m2ts_demux_del(GF_M2TS_Demuxer *ts);
void gf_m2ts_reset_parsers(GF_M2TS_Demuxer *ts);
GF_Err gf_m2ts_set_pes_framing(GF_M2TS_PES *pes, u32 mode);
GF_Err gf_m2ts_process_data(GF_M2TS_Demuxer *ts, char *data, u32 data_size);


#endif	//_GF_MPEG_TS_H_
