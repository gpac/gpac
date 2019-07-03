/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre , Cyril Concolato, Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2000-2018
 *					All rights reserved
 *
 *  This file is part of GPAC / MPEG2-TS multiplexer sub-project
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

#include <gpac/mpegts.h>
#include <gpac/constants.h>
#include <gpac/media_tools.h>

#if !defined(GPAC_DISABLE_MPEG2TS_MUX)

/* 90khz internal delay between two updates for bitrate compute per stream */
#define BITRATE_UPDATE_WINDOW	90000
/* length of adaptation_field_length; */
#define ADAPTATION_LENGTH_LENGTH 1
/* discontinuty flag, random access flag ... */
#define ADAPTATION_FLAGS_LENGTH 1
/* length of adaptation_extension_field_length; */
#define ADAPTATION_EXTENSION_LENGTH_LENGTH 1
/* AF des flags and co... */
#define ADAPTATION_EXTENSION_FLAGS_LENGTH 1
/* length of encoded pcr */
#define PCR_LENGTH 6


static GFINLINE Bool gf_m2ts_time_less(GF_M2TS_Time *a, GF_M2TS_Time *b) {
	if (a->sec>b->sec) return GF_FALSE;
	if (a->sec==b->sec) return (a->nanosec<b->nanosec) ? GF_TRUE : GF_FALSE;
	return GF_TRUE;
}
static GFINLINE Bool gf_m2ts_time_equal(GF_M2TS_Time *a, GF_M2TS_Time *b) {
	return ((a->sec==b->sec) && (a->nanosec == b->nanosec) );
}
static GFINLINE Bool gf_m2ts_time_less_or_equal(GF_M2TS_Time *a, GF_M2TS_Time *b) {
	if (a->sec>b->sec) return GF_FALSE;
	if (a->sec==b->sec) return (a->nanosec>b->nanosec) ? GF_FALSE : GF_TRUE;
	return GF_TRUE;
}

static GFINLINE void gf_m2ts_time_inc(GF_M2TS_Time *time, u64 delta_inc_num, u32 delta_inc_den)
{
	u64 n_sec;
	u64 sec;

	/*couldn't compute bitrate - we need to have more info*/
	if (!delta_inc_den) return;

	sec = delta_inc_num / delta_inc_den;

	if (sec) {
		time->sec += (u32) sec;
		sec *= delta_inc_den;
		delta_inc_num = delta_inc_num % sec;
	}
	/*move to nanosec - 0x3B9ACA00 = 1000*1000*1000 */
	n_sec = delta_inc_num;
	n_sec *= 0x3B9ACA00;
	n_sec /= delta_inc_den;
	time->nanosec += (u32) n_sec;
	while (time->nanosec >= 0x3B9ACA00) {
		time->nanosec -= 0x3B9ACA00;
		time->sec ++;
	}
}

static GFINLINE s32 gf_m2ts_time_diff_us(GF_M2TS_Time *a, GF_M2TS_Time *b)
{
	s32 drift = b->nanosec;
	drift -= a->nanosec;
	drift /= 1000;
	if (a->sec != b->sec) {
		drift += (b->sec - a->sec) * 1000000;
	}
	return drift;
}

/************************************
 * Section-related functions
 ************************************/
void gf_m2ts_mux_table_update(GF_M2TS_Mux_Stream *stream, u8 table_id, u16 table_id_extension,
                              u8 *table_payload, u32 table_payload_length,
                              Bool use_syntax_indicator, Bool private_indicator,
                              Bool use_checksum)
{
	u32 overhead_size;
	u32 offset;
	u32 section_number, nb_sections;
	GF_M2TS_Mux_Table *table, *prev_table;
	u32 maxSectionLength;
	GF_M2TS_Mux_Section *section, *prev_sec;
	GF_BitStream *bs;

	/* check if there is already a table with that id */
	prev_table = NULL;
	table = stream->tables;
	while (table) {
		if (table->table_id == table_id) {
			/* if yes, we need to flush the table and increase the version number */
			GF_M2TS_Mux_Section *sec = table->section;
			while (sec) {
				GF_M2TS_Mux_Section *sec2 = sec->next;
				gf_free(sec->data);
				gf_free(sec);
				sec = sec2;
			}
			table->version_number = (table->version_number + 1)%0x1F;
			break;
		}
		prev_table = table;
		table = table->next;
	}

	if (!table) {
		/* if no, the table is created */
		GF_SAFEALLOC(table, GF_M2TS_Mux_Table);
		if (!table) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: fail to allocate table id %d\n", stream->pid, table_id));
			return;
		}
		table->table_id = table_id;
		if (prev_table) prev_table->next = table;
		else stream->tables = table;
		table->version_number = stream->initial_version_number;
	}

	if (!table_payload_length) return;

	switch (table_id) {
	case GF_M2TS_TABLE_ID_PMT:
	case GF_M2TS_TABLE_ID_PAT:
	case GF_M2TS_TABLE_ID_SDT_ACTUAL:
	case GF_M2TS_TABLE_ID_SDT_OTHER:
	case GF_M2TS_TABLE_ID_TDT:
	case GF_M2TS_TABLE_ID_TOT:
	case GF_M2TS_TABLE_ID_BAT:
		maxSectionLength = 1024;
		break;
	case GF_M2TS_TABLE_ID_MPEG4_BIFS:
	case GF_M2TS_TABLE_ID_MPEG4_OD:
		maxSectionLength = 4096;
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Cannot create sections for table id %d\n", stream->pid, table_id));
		return;
	}

	overhead_size = SECTION_HEADER_LENGTH;
	if (use_syntax_indicator) overhead_size += SECTION_ADDITIONAL_HEADER_LENGTH + CRC_LENGTH;

	section_number = 0;
	nb_sections = 1;
	while (nb_sections*(maxSectionLength - overhead_size)<table_payload_length) nb_sections++;

	switch (table_id) {
	case GF_M2TS_TABLE_ID_PMT:
		if (nb_sections > 1)
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] last section number for PMT shall be 0\n"));
		break;
	default:
		break;
	}

	prev_sec = NULL;
	offset = 0;
	while (offset < table_payload_length) {
		u32 remain;
		GF_SAFEALLOC(section, GF_M2TS_Mux_Section);
		if (!section) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: fail to allocate section for table id %d\n", stream->pid, table_id));
			return;
		}
		remain = table_payload_length - offset;
		if (remain > maxSectionLength - overhead_size) {
			section->length = maxSectionLength;
		} else {
			section->length = remain + overhead_size;
		}

		bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);

		/* first header (not included in section length */
		gf_bs_write_int(bs,	table_id, 8);
		gf_bs_write_int(bs,	use_syntax_indicator, 1);
		gf_bs_write_int(bs,	private_indicator, 1);
		gf_bs_write_int(bs,	3, 2);  /* reserved bits are all set */
		gf_bs_write_int(bs,	section->length - SECTION_HEADER_LENGTH, 12);

		if (use_syntax_indicator) {
			/* second header */
			gf_bs_write_int(bs,	table_id_extension, 16);
			gf_bs_write_int(bs,	3, 2); /* reserved bits are all set */
			gf_bs_write_int(bs,	table->version_number, 5);
			gf_bs_write_int(bs,	1, 1); /* current_next_indicator = 1: we don't send version in advance */
			gf_bs_write_int(bs,	section_number, 8);
			section_number++;
			gf_bs_write_int(bs,	nb_sections-1, 8);
		}

		gf_bs_write_data(bs, (char *) table_payload + offset, section->length - overhead_size);
		offset += section->length - overhead_size;

		if (use_syntax_indicator) {
			/* place holder for CRC */
			gf_bs_write_u32(bs, 0);
		}

		gf_bs_get_content(bs, &section->data, &section->length);
		gf_bs_del(bs);

		if (use_syntax_indicator) {
			u32 CRC;
			CRC = gf_crc_32((char *) section->data,section->length-CRC_LENGTH);
			section->data[section->length-4] = (CRC >> 24) & 0xFF;
			section->data[section->length-3] = (CRC >> 16) & 0xFF;
			section->data[section->length-2] = (CRC >> 8) & 0xFF;
			section->data[section->length-1] = CRC & 0xFF;
		}

		if (prev_sec) prev_sec->next = section;
		else table->section = section;
		prev_sec = section;
	}
	stream->current_table = stream->tables;
	stream->current_section = stream->current_table->section;
	stream->current_section_offset = 0;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Generating %d sections for table id %d - version number %d - extension ID %d\n", stream->pid, nb_sections, table_id, table->version_number, table_id_extension));
}

void gf_m2ts_mux_table_update_bitrate(GF_M2TS_Mux *mux, GF_M2TS_Mux_Stream *stream)
{
	GF_M2TS_Mux_Table *table;

	/*update PMT*/
	if (stream->table_needs_update)
		stream->process(mux, stream);

	stream->bit_rate = 0;
	table = stream->tables;
	while (table) {
		GF_M2TS_Mux_Section *section = table->section;
		while (section) {
			u32 nb_bytes = 0;
			while (nb_bytes<section->length) nb_bytes += 188;
			stream->bit_rate += nb_bytes;
			section = section->next;
		}
		table = table->next;
	}
	stream->bit_rate *= 8;
	stream->bit_rate *= 1000;
	if (stream->refresh_rate_ms) {
		stream->bit_rate /= stream->refresh_rate_ms;
	} else if (stream->table_needs_send) {
		/*no clue ... */
		stream->bit_rate /= 100;
	} else {
		stream->bit_rate = 0;
	}
}

void gf_m2ts_mux_table_update_mpeg4(GF_M2TS_Mux_Stream *stream, u8 table_id, u16 table_id_extension,
                                    char *table_payload, u32 table_payload_length,
                                    Bool use_syntax_indicator, Bool private_indicator,
                                    Bool increment_version_number, Bool use_checksum)
{
	GF_SLHeader hdr;
	u32 overhead_size;
	u32 offset, sl_size;
	u32 section_number, nb_sections;
	GF_M2TS_Mux_Table *table, *prev_table;
	/*max section length for MPEG-4 BIFS and OD*/
	u32 maxSectionLength = 4096;
	GF_M2TS_Mux_Section *section, *prev_sec;
	GF_BitStream *bs;

	/* check if there is already a table with that id */
	prev_table = NULL;
	table = stream->tables;
	while (table) {
		if (table->table_id == table_id) {
			/* if yes, we need to flush the table and increase the version number */
			GF_M2TS_Mux_Section *sec = table->section;
			while (sec) {
				GF_M2TS_Mux_Section *sec2 = sec->next;
				gf_free(sec->data);
				gf_free(sec);
				sec = sec2;
			}
			if (increment_version_number)
				table->version_number = (table->version_number + 1)%0x1F;
			break;
		}
		prev_table = table;
		table = table->next;
	}

	if (!table) {
		/* if no, the table is created */
		GF_SAFEALLOC(table, GF_M2TS_Mux_Table);
		if (!table) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: fail to allocate table id %d\n", stream->pid, table_id));
			return;
		}
		table->table_id = table_id;
		if (prev_table) prev_table->next = table;
		else stream->tables = table;
	}

	if (!table_payload_length) return;

	overhead_size = SECTION_HEADER_LENGTH;
	if (use_syntax_indicator) overhead_size += SECTION_ADDITIONAL_HEADER_LENGTH + CRC_LENGTH;

	section_number = 0;
	nb_sections = 1;
	hdr = stream->sl_header;
	sl_size = gf_sl_get_header_size(stream->ifce->sl_config, &hdr);
	/*SL-packetized data doesn't fit in one section, we must repacketize*/
	if (sl_size + table_payload_length > maxSectionLength - overhead_size) {
		nb_sections = 0;
		offset = 0;
		hdr.accessUnitEndFlag = 0;
		while (offset<table_payload_length) {
			sl_size = gf_sl_get_header_size(stream->ifce->sl_config, &hdr);
			/*remove start flag*/
			hdr.accessUnitStartFlag = 0;
			/*fill each section but beware of last packet*/
			offset += maxSectionLength - overhead_size - sl_size;
			nb_sections++;
		}
	}
	prev_sec = NULL;
	offset = 0;
	hdr = stream->sl_header;
	while (offset < table_payload_length) {
		u32 remain;
		u8 *slhdr;
		u32 slhdr_size;
		GF_SAFEALLOC(section, GF_M2TS_Mux_Section);
		if (!section) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: fail to allocate section for table id %d\n", stream->pid, table_id));
			return;
		}

		hdr.accessUnitEndFlag = (section_number+1==nb_sections) ? stream->sl_header.accessUnitEndFlag : 0;
		gf_sl_packetize(stream->ifce->sl_config, &hdr, NULL, 0, &slhdr, &slhdr_size);
		hdr.accessUnitStartFlag = 0;

		remain = table_payload_length - offset;
		if (remain > maxSectionLength - overhead_size - slhdr_size) {
			section->length = maxSectionLength;
		} else {
			section->length = remain + overhead_size + slhdr_size;
		}
		sl_size = section->length - overhead_size - slhdr_size;

		bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);

		/* first header (not included in section length */
		gf_bs_write_int(bs,	table_id, 8);
		gf_bs_write_int(bs,	use_syntax_indicator, 1);
		gf_bs_write_int(bs,	private_indicator, 1);
		gf_bs_write_int(bs,	3, 2);  /* reserved bits are all set */
		gf_bs_write_int(bs,	section->length - SECTION_HEADER_LENGTH, 12);

		if (use_syntax_indicator) {
			/* second header */
			gf_bs_write_int(bs,	table_id_extension, 16);
			gf_bs_write_int(bs,	3, 2); /* reserved bits are all set */
			gf_bs_write_int(bs,	table->version_number, 5);
			gf_bs_write_int(bs,	1, 1); /* current_next_indicator = 1: we don't send version in advance */
			gf_bs_write_int(bs,	section_number, 8);
			section_number++;
			gf_bs_write_int(bs,	nb_sections-1, 8);
		}

		/*write sl header*/
		gf_bs_write_data(bs, slhdr, slhdr_size);
		gf_free(slhdr);
		/*write sl data*/
		gf_bs_write_data(bs, (char *) table_payload + offset, sl_size);
		offset += sl_size;

		if (use_syntax_indicator) {
			/* place holder for CRC */
			gf_bs_write_u32(bs, 0);
		}

		gf_bs_get_content(bs, &section->data, &section->length);
		gf_bs_del(bs);

		if (use_syntax_indicator) {
			u32 CRC;
			CRC = gf_crc_32((char *) section->data,section->length-CRC_LENGTH);
			section->data[section->length-4] = (CRC >> 24) & 0xFF;
			section->data[section->length-3] = (CRC >> 16) & 0xFF;
			section->data[section->length-2] = (CRC >> 8) & 0xFF;
			section->data[section->length-1] = CRC & 0xFF;
		}

		if (prev_sec) prev_sec->next = section;
		else table->section = section;
		prev_sec = section;
	}
	stream->current_table = stream->tables;
	stream->current_section = stream->current_table->section;
	stream->current_section_offset = 0;
	stream->table_needs_send = GF_TRUE;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Generating %d sections for MPEG-4 SL packet - version number %d - extension ID %d\n", stream->pid, nb_sections, table->version_number, table_id_extension));

	/*MPEG-4 tables are input streams for the mux, the bitrate is updated when fetching AUs*/
}

static u32 gf_m2ts_add_adaptation(GF_M2TS_Mux_Program *prog, GF_BitStream *bs, u16 pid,
                                  Bool has_pcr, u64 pcr_time,
                                  Bool is_rap,
                                  u32 padding_length,
                                  char *af_descriptors, u32 af_descriptors_size, Bool set_discontinuity)
{
	u32 adaptation_length;

	adaptation_length = ADAPTATION_FLAGS_LENGTH + (has_pcr?PCR_LENGTH:0) + padding_length;


	if (af_descriptors_size && af_descriptors) {
		adaptation_length += ADAPTATION_EXTENSION_LENGTH_LENGTH + ADAPTATION_EXTENSION_FLAGS_LENGTH + af_descriptors_size;
	}

	gf_bs_write_int(bs,	adaptation_length, 8);
	gf_bs_write_int(bs,	set_discontinuity ? 1 : 0, 1);			// discontinuity indicator
	gf_bs_write_int(bs,	is_rap, 1);		// random access indicator
	gf_bs_write_int(bs,	0, 1);			// es priority indicator
	gf_bs_write_int(bs,	has_pcr, 1);	// PCR_flag
	gf_bs_write_int(bs,	0, 1);			// OPCR flag
	gf_bs_write_int(bs,	0, 1);			// splicing point flag
	gf_bs_write_int(bs,	0, 1);			// transport private data flag
	gf_bs_write_int(bs,	af_descriptors_size ? 1 : 0, 1);			// adaptation field extension flag
	if (has_pcr) {
		u64 PCR_base, PCR_ext;
		PCR_base = pcr_time/300;
		gf_bs_write_long_int(bs, PCR_base, 33);
		gf_bs_write_int(bs,	0, 6); // reserved
		PCR_ext = pcr_time - PCR_base*300;
		gf_bs_write_long_int(bs, PCR_ext, 9);
		if (prog->last_pcr > pcr_time) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Sending PCR "LLD" earlier than previous PCR "LLD" - drift %f sec - discontinuity set\n", pid, pcr_time, prog->last_pcr, (prog->last_pcr - pcr_time) /27000000.0 ));
		}
		prog->last_pcr = pcr_time;

#ifndef GPAC_DISABLE_LOG
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Adding adaptation field size %d - RAP %d - Padding %d - PCR "LLD"\n", pid, adaptation_length, is_rap, padding_length, pcr_time));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Adding adaptation field size %d - RAP %d - Padding %d\n", pid, adaptation_length, is_rap, padding_length));
#endif

	}

	if (af_descriptors_size) {
		gf_bs_write_int(bs,	ADAPTATION_EXTENSION_FLAGS_LENGTH + af_descriptors_size, 8);

		gf_bs_write_int(bs,	0, 1);			// ltw_flag
		gf_bs_write_int(bs,	0, 1);			// piecewise_rate_flag
		gf_bs_write_int(bs,	0, 1);			// seamless_splice_flag
		gf_bs_write_int(bs,	0, 1);			// af_descriptor_not_present_flag
		gf_bs_write_int(bs,	0xF, 4);			// reserved

		gf_bs_write_data(bs, af_descriptors, af_descriptors_size);
	}

	gf_bs_write_byte(bs, 0xFF, padding_length); // stuffing byte

	return adaptation_length + ADAPTATION_LENGTH_LENGTH;
}

//#define USE_AF_STUFFING

void gf_m2ts_mux_table_get_next_packet(GF_M2TS_Mux *mux, GF_M2TS_Mux_Stream *stream, char *packet)
{
	GF_BitStream *bs;
	GF_M2TS_Mux_Table *table;
	GF_M2TS_Mux_Section *section;
	u32 payload_length, payload_start;
	u8 adaptation_field_control = GF_M2TS_ADAPTATION_NONE;
#ifndef USE_AF_STUFFING
	u32 padded_bytes=0;
#else
	u32 padding_length = 0;
#endif

	stream->table_needs_send = GF_FALSE;
	table = stream->current_table;
	if (!table) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] Invalid muxer state, table is NULL!\n"));
		return;
	}

	section = stream->current_section;
	assert(section);

	bs = mux->pck_bs;
	gf_bs_reassign_buffer(bs, packet, 188);

	gf_bs_write_int(bs,	0x47, 8); // sync
	gf_bs_write_int(bs,	0, 1);    // error indicator
	if (stream->current_section_offset == 0) {
		gf_bs_write_int(bs,	1, 1);    // payload start indicator
	} else {
		/* No section concatenation yet!!!*/
		gf_bs_write_int(bs,	0, 1);    // payload start indicator
	}

	if (!stream->current_section_offset) payload_length = 183;
	else payload_length = 184;

	payload_start = payload_length;

	if (section->length - stream->current_section_offset >= payload_length) {

	} else  {
		//stuffing using adaptation field - seems not well handled by some equipments ...
#ifdef USE_AF_STUFFING
		/* in all the following cases, we write an adaptation field */
		adaptation_field_control = GF_M2TS_ADAPTATION_AND_PAYLOAD;
		/* we need at least 2 bytes for adaptation field headers (no pcr) */
		payload_length -= 2;
		if (section->length - stream->current_section_offset >= payload_length) {
			padding_length = 0;
		} else {
			padding_length = payload_length - section->length + stream->current_section_offset;
			payload_length -= padding_length;
		}
#else
		//stuffing according to annex C.3
		padded_bytes = payload_length - section->length + stream->current_section_offset;
		payload_length = section->length - stream->current_section_offset;
#endif
	}

	assert(payload_length + stream->current_section_offset <= section->length);

	//CC field shall not be incremented for if adaptation field only, rewind counter
	if (adaptation_field_control == GF_M2TS_ADAPTATION_ONLY) {
		if (!stream->continuity_counter) stream->continuity_counter=15;
		else stream->continuity_counter--;
	}

	gf_bs_write_int(bs,	0, 1);    /*priority indicator*/
	gf_bs_write_int(bs,	stream->pid, 13); /*pid*/
	gf_bs_write_int(bs,	0, 2);    /*scrambling indicator*/
	gf_bs_write_int(bs,	adaptation_field_control, 2);    /*we do not use adaptation field for sections */
	gf_bs_write_int(bs,	stream->continuity_counter, 4);   /*continuity counter*/

	if (stream->continuity_counter < 15) stream->continuity_counter++;
	else stream->continuity_counter=0;

#ifdef USE_AF_STUFFING
	if (adaptation_field_control != GF_M2TS_ADAPTATION_NONE)
		gf_m2ts_add_adaptation(stream->program, bs, stream->pid, 0, 0, 0, padding_length, NULL, 0, GF_FALSE);
#endif

	/*pointer field*/
	if (!stream->current_section_offset) {
		/* no concatenations of sections in ts packets, so start address is 0 */
		gf_bs_write_u8(bs, 0);
	}

	memcpy(packet+188-payload_start, section->data + stream->current_section_offset, payload_length);
	stream->current_section_offset += payload_length;
#ifndef USE_AF_STUFFING
	if (padded_bytes) memset(packet+188-payload_start+payload_length, 0xFF, padded_bytes);
#endif

	if (stream->current_section_offset == section->length) {
		stream->current_section_offset = 0;
		stream->current_section = stream->current_section->next;
		if (!stream->current_section) {
			stream->current_table = stream->current_table->next;
			/*carousel table*/
			if (!stream->current_table) {
				if (stream->ifce) stream->refresh_rate_ms = stream->ifce->repeat_rate;
				if (stream->refresh_rate_ms) {
					stream->current_table = stream->tables;
					/*update ES time*/
					gf_m2ts_time_inc(&stream->time, stream->refresh_rate_ms, 1000);
				}
			}
			if (stream->current_table) stream->current_section = stream->current_table->section;
		}
	}
	/*updates number of bytes sent for bitrate compute (MPEG4 sections)*/
	stream->bytes_since_last_time += 188;
}


u32 gf_m2ts_stream_process_sdt(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
{
	if (stream->table_needs_update) { /* generate table payload */
		GF_M2TS_Mux_Program *prog;
		GF_BitStream *bs;
		u8 *payload;
		u32 size;

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

		gf_bs_write_u16(bs, muxer->ts_id);
		gf_bs_write_u8(bs, 0xFF);	//reserved future use

		prog = muxer->programs;
		while (prog) {
			u32 len = 0;
			gf_bs_write_u16(bs, prog->number);
			gf_bs_write_int(bs, 0xFF, 6); //reserved future use
			gf_bs_write_int(bs, 0, 1); //EIT_schedule_flag
			gf_bs_write_int(bs, 0, 1); //EIT_present_following_flag
			gf_bs_write_int(bs, 4, 3); //running status
			gf_bs_write_int(bs, 0, 1); //free_CA_mode

			if (prog->name) len += (u32) strlen(prog->name);
			if (prog->provider) len += (u32) strlen(prog->provider);

			if (len) {
				len += 3;
				gf_bs_write_int(bs, len + 2, 12);
				gf_bs_write_u8(bs, GF_M2TS_DVB_SERVICE_DESCRIPTOR);
				gf_bs_write_u8(bs, len);
				gf_bs_write_u8(bs, 0x01);
				len = prog->provider ? (u32) strlen(prog->provider) : 0;
				gf_bs_write_u8(bs, len);
				if (prog->provider) gf_bs_write_data(bs, prog->provider, len);

				len = prog->name ? (u32) strlen(prog->name) : 0;
				gf_bs_write_u8(bs, len);
				if (prog->name) gf_bs_write_data(bs, prog->name, len);
			} else {
				gf_bs_write_int(bs, 0, 12);
			}
			prog = prog->next;
		}
		gf_bs_get_content(bs, &payload, &size);
		gf_bs_del(bs);
		gf_m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_SDT_ACTUAL, muxer->ts_id, payload, size, GF_TRUE, GF_FALSE, GF_FALSE);
		stream->table_needs_update = GF_FALSE;
		stream->table_needs_send = GF_TRUE;
		gf_free(payload);
	}
	if (stream->table_needs_send)
		return 1;
	if (stream->refresh_rate_ms)
		return 1;
	return 0;
}

u32 gf_m2ts_stream_process_pat(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
{
	if (stream->table_needs_update) { /* generate table payload */
		GF_M2TS_Mux_Program *prog;
		GF_BitStream *bs;
		u8 *payload;
		u32 size;

		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		prog = muxer->programs;
		while (prog) {
			gf_bs_write_u16(bs, prog->number);
			gf_bs_write_int(bs, 0x7, 3);	/*reserved*/
			gf_bs_write_int(bs, prog->pmt->pid, 13);	/*reserved*/
			prog = prog->next;
		}
		gf_bs_get_content(bs, &payload, &size);
		gf_bs_del(bs);
		gf_m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PAT, muxer->ts_id, payload, size, GF_TRUE, GF_FALSE, GF_FALSE);
		stream->table_needs_update = GF_FALSE;
		stream->table_needs_send = GF_TRUE;
		gf_free(payload);
	}
	if (stream->table_needs_send)
		return 1;
	if (stream->refresh_rate_ms)
		return 1;
	return 0;
}

u32 gf_m2ts_stream_process_pmt(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
{
	if (stream->table_needs_update) { /* generate table payload */
		GF_M2TS_Mux_Stream *es;
		u8 *payload;
		u32 i;
		u32 length, nb_streams=0;
		u32 info_length = 0, es_info_length = 0;
		GF_BitStream *bs;


		bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs,	0x7, 3); // reserved
		gf_bs_write_int(bs,	stream->program->pcr->pid, 13);
		gf_bs_write_int(bs,	0xF, 4); // reserved


		if (stream->program->loop_descriptors) {
			for (i=0; i<gf_list_count(stream->program->loop_descriptors); i++) {
				GF_M2TSDescriptor *desc = (GF_M2TSDescriptor*)gf_list_get(stream->program->loop_descriptors, i);
				info_length += 2 + desc->data_len;
			}
		}

		if (!stream->program->iod) {
			gf_bs_write_int(bs,	info_length, 12); // program info length =0
		} else {
			u32 len, i;
			GF_ESD *esd;
			GF_BitStream *bs_iod;
			u8 *iod_data;
			u32 iod_data_len;

			/*rewrite SL config in IOD streams*/
			i=0;
			while (NULL != (esd = (GF_ESD*)gf_list_enum(((GF_ObjectDescriptor*)stream->program->iod)->ESDescriptors, &i))) {
				GF_M2TS_Mux_Stream *es_stream = stream->program->streams;
				while (es_stream) {
					if (es_stream->ifce && (es_stream->ifce->stream_id==esd->ESID)) {
						/*thay should be the same ...*/
						memcpy(esd->slConfig, es_stream->ifce->sl_config, sizeof(GF_SLConfig));
						break;
					}
					es_stream = es_stream->next;
				}
			}

			bs_iod = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);
			gf_odf_write_descriptor(bs_iod, stream->program->iod);
			gf_bs_get_content(bs_iod, &iod_data, &iod_data_len);
			gf_bs_del(bs_iod);

			len = iod_data_len + 4;
			gf_bs_write_int(bs,	len + info_length, 12); // program info length

			gf_bs_write_int(bs,	GF_M2TS_MPEG4_IOD_DESCRIPTOR, 8);
			len = iod_data_len + 2;
			gf_bs_write_int(bs,	len, 8);

			/* Scope_of_IOD_label :
				0x10 iod unique a l'int�rieur de programme
				0x11 iod unoque dans le flux ts */
			gf_bs_write_int(bs,	2, 8);

			gf_bs_write_int(bs,	2, 8);  // IOD_label

			gf_bs_write_data(bs, iod_data, iod_data_len);
			gf_free(iod_data);
		}

		/*write all other descriptors*/
		if (stream->program->loop_descriptors) {
			for (i=0; i<gf_list_count(stream->program->loop_descriptors); i++) {
				GF_M2TSDescriptor *desc = (GF_M2TSDescriptor*)gf_list_get(stream->program->loop_descriptors, i);
				gf_bs_write_int(bs,	desc->tag, 8);
				gf_bs_write_int(bs,	desc->data_len, 8);
				gf_bs_write_data(bs, desc->data, desc->data_len);
			}
		}

		es = stream->program->streams;
		while (es) {
			Bool has_lang = GF_FALSE;
			u8 type = es->mpeg2_stream_type;
			nb_streams++;
			es_info_length = 0;


			switch (es->mpeg2_stream_type) {
			case GF_M2TS_AUDIO_AC3:
			case GF_M2TS_VIDEO_VC1:
				es_info_length += 2 + 4;
				type = GF_M2TS_PRIVATE_DATA;
				break;
			case GF_M2TS_AUDIO_EC3:
				es_info_length += 2;
				type = GF_M2TS_PRIVATE_DATA;
				break;
			default:
				if (es->force_reg_desc) {
					es_info_length += 2 + 4 + 4;
					type = GF_M2TS_PRIVATE_DATA;
				}
				break;
			}

			gf_bs_write_int(bs,	type, 8);
			gf_bs_write_int(bs,	0x7, 3); // reserved
			gf_bs_write_int(bs,	es->pid, 13);
			gf_bs_write_int(bs,	0xF, 4); // reserved

			/*calculate es_info_length*/
			if (stream->program->iod && !(es->ifce->caps & GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS))
				es_info_length += 4;

			/*another loop descriptors*/
			if (es->loop_descriptors)
			{
				for (i=0; i<gf_list_count(es->loop_descriptors); i++)
				{
					GF_M2TSDescriptor *desc = (GF_M2TSDescriptor*)gf_list_get(es->loop_descriptors, i);
					es_info_length += 2 +desc->data_len;
				}
			}

			if (es->ifce && es->ifce->lang && (es->ifce->lang != GF_LANG_UNKNOWN) ) {
				es_info_length += 2 + 3;
				has_lang = GF_TRUE;
			}

			gf_bs_write_int(bs,	es_info_length, 12);

			if (stream->program->iod && !(es->ifce->caps & GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS)) {
				gf_bs_write_int(bs,	GF_M2TS_MPEG4_SL_DESCRIPTOR, 8);
				gf_bs_write_int(bs,	2, 8);
				gf_bs_write_int(bs,	es->ifce->stream_id, 16);  // mpeg4_esid
			}

			if (has_lang) {
				gf_bs_write_int(bs,	GF_M2TS_ISO_639_LANGUAGE_DESCRIPTOR, 8);
				gf_bs_write_int(bs,	3, 8);
				gf_bs_write_int(bs,	(es->ifce->lang>>24) & 0xFF, 8);
				gf_bs_write_int(bs,	(es->ifce->lang>>16) & 0xFF, 8);
				gf_bs_write_int(bs,	es->ifce->lang & 0xFF, 8);
			}

			switch (es->mpeg2_stream_type) {
			case GF_M2TS_AUDIO_AC3:
				gf_bs_write_int(bs,	GF_M2TS_REGISTRATION_DESCRIPTOR, 8);
				gf_bs_write_int(bs,	4, 8);
				gf_bs_write_int(bs,	'A', 8);
				gf_bs_write_int(bs,	'C', 8);
				gf_bs_write_int(bs,	'-', 8);
				gf_bs_write_int(bs,	'3', 8);
				break;
			case GF_M2TS_VIDEO_VC1:
				gf_bs_write_int(bs,	GF_M2TS_REGISTRATION_DESCRIPTOR, 8);
				gf_bs_write_int(bs,	4, 8);
				gf_bs_write_int(bs,	'V', 8);
				gf_bs_write_int(bs,	'C', 8);
				gf_bs_write_int(bs,	'-', 8);
				gf_bs_write_int(bs,	'1', 8);
				break;
			case GF_M2TS_AUDIO_EC3:
				gf_bs_write_int(bs,	GF_M2TS_DVB_EAC3_DESCRIPTOR, 8);
				gf_bs_write_int(bs,	0, 8); //check what is in this desc
				break;
			default:
				if (es->force_reg_desc) {
					gf_bs_write_int(bs,	GF_M2TS_REGISTRATION_DESCRIPTOR, 8);
					gf_bs_write_int(bs,	8, 8);
					gf_bs_write_int(bs,	GF_M2TS_RA_STREAM_GPAC, 32);
					gf_bs_write_int(bs,	es->ifce->codecid, 32);
				}
				break;
			}

			if (es->loop_descriptors)
			{
				for (i=0; i<gf_list_count(es->loop_descriptors); i++)
				{
					GF_M2TSDescriptor *desc = (GF_M2TSDescriptor *)gf_list_get(es->loop_descriptors, i);
					gf_bs_write_int(bs,	desc->tag, 8);
					gf_bs_write_int(bs,	desc->data_len, 8);
					gf_bs_write_data(bs, desc->data, desc->data_len);
				}
			}

			es = es->next;
		}

		gf_bs_get_content(bs, &payload, &length);
		gf_bs_del(bs);

		gf_m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PMT, stream->program->number, payload, length, GF_TRUE, GF_FALSE, GF_FALSE);
		stream->table_needs_update = GF_FALSE;
		stream->table_needs_send = GF_TRUE;
		gf_free(payload);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Updating PMT - Program Number %d - %d streams - size %d%s\n", stream->pid, stream->program->number, nb_streams, length, stream->program->iod ? " - MPEG-4 Systems detected":""));
	}
	if (stream->table_needs_send)
		return 1;
	if (stream->refresh_rate_ms)
		return 1;
	return 0;
}

static void gf_m2ts_remap_timestamps_for_pes(GF_M2TS_Mux_Stream *stream, u32 pck_flags, u64 *dts, u64 *cts, u32 *duration)
{
	u64 pcr_offset;

	if (*dts > *cts) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: DTS "LLD" is greater than CTS "LLD" (like ISOBMF CTTSv1 input) - adjusting to CTS\n", stream->pid, *dts, *cts));
		*dts = *cts;
	}

	/*Rescale our timestamps and express them in PCR*/
	if (stream->ts_scale.den) {
		*cts = *cts * stream->ts_scale.num / stream->ts_scale.den ;
		*dts = *dts * stream->ts_scale.num / stream->ts_scale.den ;
		if (duration) *duration = (u32)( *duration * stream->ts_scale.num / stream->ts_scale.den ) ;
	}
	if (!stream->program->initial_ts_set) {
		u32 nb_bits = (u32) (stream->program->mux->tot_pck_sent - stream->program->num_pck_at_pcr_init) * 1504;
		u64 nb_ticks = 90000*nb_bits / stream->program->mux->bit_rate;
		stream->program->initial_ts = *dts;

		if (stream->program->initial_ts > nb_ticks)
			stream->program->initial_ts -= nb_ticks;
		else
			stream->program->initial_ts = 0;

		stream->program->initial_ts_set = GF_TRUE;
	}
	else if (*dts < stream->program->initial_ts) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: DTS "LLD" is less than initial DTS "LLD" - adjusting\n", stream->pid, *dts, stream->program->initial_ts));
		stream->program->initial_ts = *dts;
	}
	else if (*dts < stream->last_dts) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: DTS "LLD" is less than last sent DTS "LLD"\n", stream->pid, *dts, stream->last_dts));
		stream->last_dts = *dts;
	} else {
		stream->last_dts = *dts;
	}

	/*offset our timestamps*/
	*cts += stream->program->pcr_offset;
	*dts += stream->program->pcr_offset;

	/*PCR offset, in 90000 hz not in 270000000*/
	pcr_offset = stream->program->pcr_init_time/300;
	*cts = *cts - stream->program->initial_ts + pcr_offset;
	*dts = *dts - stream->program->initial_ts + pcr_offset;
}

void id3_write_size(GF_BitStream *bs, u32 len)
{
	u32 size;

	size = (len>>21) & 0x7F;
	gf_bs_write_int(bs, 0, 1);
	gf_bs_write_int(bs, size, 7);

	size = (len>>14) & 0x7F;
	gf_bs_write_int(bs, 0, 1);
	gf_bs_write_int(bs, size, 7);

	size = (len>>7) & 0x7F;
	gf_bs_write_int(bs, 0, 1);
	gf_bs_write_int(bs, size, 7);

	size = (len) & 0x7F;
	gf_bs_write_int(bs, 0, 1);
	gf_bs_write_int(bs, size, 7);
}

static void id3_tag_create(u8 **input, u32 *len)
{
	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u8(bs, 'I');
	gf_bs_write_u8(bs, 'D');
	gf_bs_write_u8(bs, '3');
	gf_bs_write_u8(bs, 4); //major
	gf_bs_write_u8(bs, 0); //minor
	gf_bs_write_u8(bs, 0); //flags

	id3_write_size(bs, *len + 10);

	gf_bs_write_u32(bs, ID3V2_FRAME_TXXX);
	id3_write_size(bs, *len); /* size of the text */
	gf_bs_write_u8(bs, 0);
	gf_bs_write_u8(bs, 0);
	gf_bs_write_data(bs, *input, *len);
	gf_free(*input);
	gf_bs_get_content(bs, input, len);
	gf_bs_del(bs);
}

static Bool gf_m2ts_adjust_next_stream_time_for_pcr(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
{
	u32 pck_diff;
	s32 us_diff;
	GF_M2TS_Time next_pcr_time, stream_time;

	if (!muxer->enable_forced_pcr) return 1;

	if (!muxer->bit_rate) return 1;

	next_pcr_time = stream->program->ts_time_at_pcr_init;
	pck_diff = (u32) (stream->program->nb_pck_last_pcr - stream->program->num_pck_at_pcr_init);
	gf_m2ts_time_inc(&next_pcr_time, pck_diff*1504, stream->program->mux->bit_rate);
	gf_m2ts_time_inc(&next_pcr_time, stream->program->mux->pcr_update_ms, 1000);

	stream_time = stream->pcr_only_mode ? stream->next_time : stream->time;
	//If next_pcr_time < stream->time, we need to inject pure pcr data
	us_diff = gf_m2ts_time_diff_us(&next_pcr_time, &stream_time);
	if (us_diff > 0) {
		if (!stream->pcr_only_mode) {
			stream->pcr_only_mode = GF_TRUE;
			stream->next_time = stream->time;
		}
		stream->time = next_pcr_time;
		/*if too ahead of mux time, don't insert PCR*/
		us_diff = gf_m2ts_time_diff_us(&stream->program->mux->time, &stream->time);
		if (us_diff>1000)
			return 0;
	} else if (stream->pcr_only_mode) {
		stream->pcr_only_mode = GF_FALSE;
		stream->time = stream->next_time;
	}
	return 1;
}

static u32 gf_m2ts_stream_process_pes(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
{
	u64 time_inc;
	Bool ret = GF_FALSE;

	if (stream->mpeg2_stream_type==GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
		/*section has just been updated */
		if (stream->table_needs_send)
			return stream->scheduling_priority;
		/*section is not completely sent yet or this is not the first section of the table*/
		if (stream->current_section && (stream->current_section_offset || stream->current_section!=stream->current_table->section))
			return stream->scheduling_priority;
		if (stream->ifce->repeat_rate && stream->tables)
			ret = stream->program->pcr_init_time ? stream->scheduling_priority : GF_FALSE;
	}
	/*PES packet not completely sent yet*/
	else if (stream->curr_pck.data_len && stream->pck_offset < stream->curr_pck.data_len) {
		//if in pure PCR mode, check if we can fall back to regular mode and start sending the stream
		if ((stream == stream->program->pcr) && stream->pcr_only_mode) {
			if (! gf_m2ts_adjust_next_stream_time_for_pcr(muxer, stream)) {
				return 0;
			}
		}
		return stream->scheduling_priority;
	}

	/*PULL mode*/
	if (stream->ifce->caps & GF_ESI_AU_PULL_CAP) {
		if (stream->curr_pck.data_len) {
			/*discard packet data if we use SL over PES*/
			if (stream->discard_data) gf_free(stream->curr_pck.data);
			if (stream->curr_pck.mpeg2_af_descriptors) gf_free(stream->curr_pck.mpeg2_af_descriptors);
			/*release data*/
			stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_RELEASE, NULL);
		}
		stream->pck_offset = 0;
		stream->curr_pck.data_len = 0;
		stream->discard_data = GF_FALSE;

		/*EOS*/
		if (stream->ifce->caps & GF_ESI_STREAM_IS_OVER) return ret;
		assert(stream->ifce->input_ctrl);
		stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_PULL, &stream->curr_pck);
	} else {
		GF_M2TS_Packet *curr_pck;

		if (!stream->pck_first && (stream->ifce->caps & GF_ESI_STREAM_IS_OVER))
			return ret;

		/*flush input pipe*/
		if (stream->ifce->input_ctrl) stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_FLUSH, NULL);

		if (stream->mx) gf_mx_p(stream->mx);

		stream->pck_offset = 0;
		stream->curr_pck.data_len = 0;

		/*fill curr_pck*/
		curr_pck = stream->pck_first;
		if (!curr_pck) {
			if (stream->mx) gf_mx_v(stream->mx);
			return ret;
		}
		stream->curr_pck.cts = curr_pck->cts;
		stream->curr_pck.data = curr_pck->data;
		stream->curr_pck.data_len = curr_pck->data_len;
		stream->curr_pck.dts = curr_pck->dts;
		stream->curr_pck.duration = curr_pck->duration;
		stream->curr_pck.flags = curr_pck->flags;
		stream->curr_pck.sap_type = curr_pck->sap_type;
		stream->curr_pck.mpeg2_af_descriptors = curr_pck->mpeg2_af_descriptors;
		stream->curr_pck.mpeg2_af_descriptors_size = curr_pck->mpeg2_af_descriptors_size;

		/*discard first packet*/
		stream->pck_first = curr_pck->next;
		gf_free(curr_pck);
		stream->discard_data = GF_TRUE;

		if (stream->mx) gf_mx_v(stream->mx);
	}

	if (!(stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS))
		stream->curr_pck.dts = stream->curr_pck.cts;

	/*initializing the PCR*/
	if (!stream->program->pcr_init_time_set) {
		if (stream==stream->program->pcr) {
			if (stream->program->mux->init_pcr_value) {
				stream->program->pcr_init_time = stream->program->mux->init_pcr_value-1;
			} else {
				while (!stream->program->pcr_init_time)
					stream->program->pcr_init_time = gf_rand();
			}
			stream->program->pcr_init_time_set = GF_TRUE;
			stream->program->ts_time_at_pcr_init = muxer->time;
			stream->program->num_pck_at_pcr_init = muxer->tot_pck_sent;

			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Initializing PCR for program number %d: PCR %d - mux time %d:%09d\n", stream->pid, stream->program->number, stream->program->pcr_init_time, muxer->time.sec, muxer->time.nanosec));
		} else {
			/*PES has been sent, discard internal buffer*/
			if (stream->discard_data) gf_free(stream->curr_pck.data);
			if (stream->curr_pck.mpeg2_af_descriptors) gf_free(stream->curr_pck.mpeg2_af_descriptors);
			stream->curr_pck.data = NULL;
			stream->curr_pck.data_len = 0;
			stream->curr_pck.mpeg2_af_descriptors = NULL;
			stream->curr_pck.mpeg2_af_descriptors_size = 0;
			stream->pck_offset = 0;
			/*don't send until PCR is initialized*/
			return 0;
		}
	}

	/*SL-encapsultaion*/
	switch (stream->mpeg2_stream_type) {
	case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
		/*update SL config*/
		stream->sl_header.accessUnitStartFlag = (stream->curr_pck.flags & GF_ESI_DATA_AU_START) ? 1 : 0;
		stream->sl_header.accessUnitEndFlag = (stream->curr_pck.flags & GF_ESI_DATA_AU_END) ? 1 : 0;
#if 0
		assert(stream->sl_header.accessUnitLength + stream->curr_pck.data_len < 65536); /*stream->sl_header.accessUnitLength type is u16*/
		stream->sl_header.accessUnitLength += stream->curr_pck.data_len;
#endif
		stream->sl_header.randomAccessPointFlag = (stream->curr_pck.sap_type) ? 1: 0;
		stream->sl_header.compositionTimeStampFlag = (stream->curr_pck.flags & GF_ESI_DATA_HAS_CTS) ? 1 : 0;
		stream->sl_header.compositionTimeStamp = stream->curr_pck.cts;
		stream->sl_header.decodingTimeStampFlag = (stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS) ? 1: 0;
		stream->sl_header.decodingTimeStamp = stream->curr_pck.dts;

		gf_m2ts_mux_table_update_mpeg4(stream, stream->table_id, muxer->ts_id, stream->curr_pck.data, stream->curr_pck.data_len, GF_TRUE, GF_FALSE, (stream->curr_pck.flags & GF_ESI_DATA_REPEAT) ? GF_FALSE : GF_TRUE, GF_FALSE);

		/*packet data is now copied in sections, discard it if not pull*/
		if (!(stream->ifce->caps & GF_ESI_AU_PULL_CAP)) {
			gf_free(stream->curr_pck.data);
			stream->curr_pck.data = NULL;
			stream->curr_pck.data_len = 0;
			gf_free(stream->curr_pck.mpeg2_af_descriptors);
			stream->curr_pck.mpeg2_af_descriptors = NULL;
			stream->curr_pck.mpeg2_af_descriptors_size = 0;
		}
		break;
	case GF_M2TS_SYSTEMS_MPEG4_PES:
	{
		char *src_data;
		u32 src_data_len;

		/*update SL config*/
		stream->sl_header.accessUnitStartFlag = (stream->curr_pck.flags & GF_ESI_DATA_AU_START) ? 1 : 0;
		stream->sl_header.accessUnitEndFlag = (stream->curr_pck.flags & GF_ESI_DATA_AU_END) ? 1 : 0;
#if 0
		assert(stream->sl_header.accessUnitLength + stream->curr_pck.data_len < 65536); /*stream->sl_header.accessUnitLength type is u16*/
		stream->sl_header.accessUnitLength += stream->curr_pck.data_len;
#endif
		stream->sl_header.randomAccessPointFlag = (stream->curr_pck.sap_type) ? 1: 0;
		stream->sl_header.compositionTimeStampFlag = (stream->curr_pck.flags & GF_ESI_DATA_HAS_CTS) ? 1 : 0;
		stream->sl_header.compositionTimeStamp = stream->curr_pck.cts;
		stream->sl_header.decodingTimeStampFlag = (stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS) ? 1: 0;
		stream->sl_header.decodingTimeStamp = stream->curr_pck.dts;

		src_data = stream->curr_pck.data;
		src_data_len = stream->curr_pck.data_len;
		stream->curr_pck.data_len = 0;
		stream->curr_pck.data = NULL;

		gf_sl_packetize(stream->ifce->sl_config, &stream->sl_header, src_data, src_data_len, &stream->curr_pck.data, &stream->curr_pck.data_len);

		/*discard src data*/
		if (!(stream->ifce->caps & GF_ESI_AU_PULL_CAP)) {
			gf_free(src_data);
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Encapsulating MPEG-4 SL Data (%p - %p) on PES - SL Header size %d\n", stream->pid, src_data, stream->curr_pck.data, stream->curr_pck.data_len - src_data_len));

		/*moving from PES to SL reallocates a new buffer, force discard even in pull mode*/
		stream->discard_data = GF_TRUE;
	}
	break;
	/*perform LATM encapsulation*/
	case GF_M2TS_AUDIO_LATM_AAC:
	{
		u32 size, next_time;
		GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs, 0x2B7, 11);
		gf_bs_write_int(bs, 0, 13);

		/*same mux config = 0 (refresh aac config)*/
		next_time = gf_sys_clock();
		if (stream->ifce->decoder_config && (stream->last_aac_time + stream->ifce->repeat_rate < next_time)) {
			GF_M4ADecSpecInfo cfg;
			stream->last_aac_time = next_time;

			gf_bs_write_int(bs, 0, 1);
			/*mux config */
			gf_bs_write_int(bs, 0, 1);/*audio mux version = 0*/
			gf_bs_write_int(bs, 1, 1);/*allStreamsSameTimeFraming*/
			gf_bs_write_int(bs, 0, 6);/*numSubFrames*/
			gf_bs_write_int(bs, 0, 4);/*numProgram*/
			gf_bs_write_int(bs, 0, 3);/*numLayer prog 1*/

			gf_m4a_get_config(stream->ifce->decoder_config, stream->ifce->decoder_config_size, &cfg);
			gf_m4a_write_config_bs(bs, &cfg);

			gf_bs_write_int(bs, 0, 3);/*frameLengthType*/
			gf_bs_write_int(bs, 0, 8);/*latmBufferFullness*/
			gf_bs_write_int(bs, 0, 1);/*other data present*/
			gf_bs_write_int(bs, 0, 1);/*crcCheckPresent*/
		} else {
			gf_bs_write_int(bs, 1, 1);
		}
		/*write payloadLengthInfo*/
		size = stream->curr_pck.data_len;
		while (1) {
			if (size>=255) {
				gf_bs_write_int(bs, 255, 8);
				size -= 255;
			} else {
				gf_bs_write_int(bs, size, 8);
				break;
			}
		}
		stream->reframe_overhead = stream->curr_pck.data_len;
		gf_bs_write_data(bs, stream->curr_pck.data, stream->curr_pck.data_len);
		gf_bs_align(bs);
		gf_free(stream->curr_pck.data);
		gf_bs_get_content(bs, &stream->curr_pck.data, &stream->curr_pck.data_len);
		gf_bs_del(bs);
		stream->reframe_overhead = stream->curr_pck.data_len - stream->reframe_overhead;

		/*rewrite LATM frame header*/
		size = stream->curr_pck.data_len - 2;
		stream->curr_pck.data[1] |= (size>>8) & 0x1F;
		stream->curr_pck.data[2] = (size) & 0xFF;
		/*since we reallocated the packet data buffer, force a discard in pull mode*/
		stream->discard_data = GF_TRUE;
	}
	break;
	/*perform ADTS encapsulation*/
	case GF_M2TS_AUDIO_AAC:
		if (stream->ifce->decoder_config) {
			GF_M4ADecSpecInfo cfg;
			GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_m4a_get_config(stream->ifce->decoder_config, stream->ifce->decoder_config_size, &cfg);

			if (cfg.base_object_type>=5) cfg.base_object_type = GF_M4A_AAC_LC;

			gf_bs_write_int(bs, 0xFFF, 12);/*sync*/
			gf_bs_write_int(bs, 0, 1);/*mpeg2 aac*/
			gf_bs_write_int(bs, 0, 2); /*layer*/
			gf_bs_write_int(bs, 1, 1); /* protection_absent*/
			gf_bs_write_int(bs, cfg.base_object_type-1, 2);
			gf_bs_write_int(bs, cfg.base_sr_index, 4);
			gf_bs_write_int(bs, 0, 1);
			gf_bs_write_int(bs, cfg.nb_chan, 3);
			gf_bs_write_int(bs, 0, 4);
			gf_bs_write_int(bs, 7+stream->curr_pck.data_len, 13);
			gf_bs_write_int(bs, 0x7FF, 11);
			gf_bs_write_int(bs, 0, 2);

			gf_bs_write_data(bs, stream->curr_pck.data, stream->curr_pck.data_len);
			gf_bs_align(bs);
			gf_free(stream->curr_pck.data);
			gf_bs_get_content(bs, &stream->curr_pck.data, &stream->curr_pck.data_len);
			gf_bs_del(bs);
			/*constant reframe overhead*/
			stream->reframe_overhead = 7;
		}
		/*since we reallocated the packet data buffer, force a discard in pull mode*/
		stream->discard_data = GF_TRUE;
		break;
	case GF_M2TS_METADATA_PES:
	case GF_M2TS_METADATA_ID3_HLS:
	{
		id3_tag_create(&stream->curr_pck.data, &stream->curr_pck.data_len);
		stream->discard_data = GF_TRUE;
	}
	break;
	}

	if (stream->start_pes_at_rap && (stream->curr_pck.sap_type)
	   ) {
		stream->program->mux->force_pat_pmt_state = GF_SEG_BOUNDARY_FORCE_PAT;
		stream->program->mux->force_pat = GF_TRUE;
	}

	/*rewrite timestamps for PES header*/
	gf_m2ts_remap_timestamps_for_pes(stream, stream->curr_pck.flags, &stream->curr_pck.dts, &stream->curr_pck.cts, &stream->curr_pck.duration);

	/*compute next interesting time in TS unit: this will be DTS of next packet*/
	stream->time = stream->program->ts_time_at_pcr_init;
	time_inc = stream->curr_pck.dts - stream->program->pcr_init_time/300;

	gf_m2ts_time_inc(&stream->time, time_inc, 90000);

	if (stream == stream->program->pcr) {
		gf_m2ts_adjust_next_stream_time_for_pcr(muxer, stream);
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Next data schedule for %d:%09d - mux time %d:%09d\n", stream->pid, stream->time.sec, stream->time.nanosec, muxer->time.sec, muxer->time.nanosec));

	/*compute instant bitrate*/
	if (!stream->last_br_time) {
		stream->last_br_time = stream->curr_pck.dts + 1;
		stream->bytes_since_last_time = 0;
		stream->pes_since_last_time = 0;
	} else {
		u32 time_diff = (u32) (stream->curr_pck.dts + 1 - stream->last_br_time );
		if ((stream->pes_since_last_time > 4) && (time_diff >= BITRATE_UPDATE_WINDOW)) {
			u32 bitrate;
			u64 r = 8*stream->bytes_since_last_time;
			r*=90000;
			bitrate = (u32) (r / time_diff);

			if (stream->program->mux->fixed_rate || (stream->bit_rate < bitrate)) {
				stream->bit_rate = bitrate;
				stream->program->mux->needs_reconfig = GF_TRUE;
			}
			stream->last_br_time = 0;
			stream->bytes_since_last_time = 0;
			stream->pes_since_last_time = 0;
		}
	}

	stream->pes_since_last_time ++;
	return stream->scheduling_priority;
}

static GFINLINE u64 gf_m2ts_get_pcr(GF_M2TS_Mux_Stream *stream)
{
	u64 pcr;
	/*compute PCR*/
	if (stream->program->mux->fixed_rate) {
		Double abs_pcr = (Double) (stream->program->mux->tot_pck_sent - stream->program->num_pck_at_pcr_init);
		abs_pcr *= 27000000;
		abs_pcr *= 1504;
		abs_pcr /= stream->program->mux->bit_rate;
		pcr = (u64) abs_pcr + stream->program->pcr_init_time;
	}
	/*in non-realtime mode with no fixed rate we only insert PCR based on DTS */
	else {
		pcr = (stream->curr_pck.dts - stream->program->pcr_offset) * 300;
	}
	return pcr;
}

void gf_m2ts_stream_update_data_following(GF_M2TS_Mux_Stream *stream)
{
	Bool ignore_next = GF_FALSE;
	stream->next_payload_size = 0;
	stream->next_next_payload_size = 0;

	stream->next_pck_flags = 0;
	stream->next_pck_sap = 0;
	stream->copy_from_next_packets = 0;

	//AU packing in single PES is disabled
	if (stream->force_single_au) return;

	if (stream->ifce->caps & GF_ESI_AU_PULL_CAP) {
		GF_ESIPacket test_pck;
		test_pck.data_len = 0;
		/*pull next data but do not release it since it might be needed later on*/
		stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_PULL, &test_pck);
		if (test_pck.data_len) {
			stream->next_payload_size = test_pck.data_len;
			stream->next_pck_flags = test_pck.flags;
			stream->next_pck_sap = test_pck.sap_type;
			stream->next_pck_cts = test_pck.cts;
			stream->next_pck_dts = test_pck.dts;
		}
	} else {
		/*flush input*/
		if (!stream->pck_first && stream->ifce->input_ctrl) stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_FLUSH, NULL);
		if (stream->pck_first) {
			stream->next_payload_size = stream->pck_first->data_len;
			stream->next_pck_cts = stream->pck_first->cts;
			stream->next_pck_dts = stream->pck_first->dts;
			stream->next_pck_flags = stream->pck_first->flags;
			stream->next_pck_sap = stream->pck_first->sap_type;

			if (!stream->pck_first->next && stream->ifce->input_ctrl) stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_FLUSH, NULL);
			if (stream->pck_first->next) {
				stream->next_next_payload_size = stream->pck_first->next->data_len;
			}

		}
	}
	/*consider we don't have the next AU if:
	1- we are asked to start new PES at RAP, just consider we don't have the next AU*/
	if (stream->start_pes_at_rap && stream->next_pck_sap) {
		ignore_next = GF_TRUE;
//		stream->program->mux->force_pat_pmt_state = GF_SEG_BOUNDARY_START;
	}
	/*if we have a RAP about to start on a stream in this program, force all other streams to stop merging data in their current PES*/
	else if (stream->program->mux->force_pat_pmt_state) {
		ignore_next = GF_TRUE;
	}

	if (ignore_next) {
		stream->next_payload_size = 0;
		stream->next_pck_cts = 0;
		stream->next_pck_dts = 0;
		stream->next_pck_flags = 0;
		stream->next_pck_sap = 0;
	}

	if (stream->next_payload_size) {
		stream->next_payload_size += stream->reframe_overhead;
		if (stream->next_next_payload_size)
			stream->next_next_payload_size += stream->reframe_overhead;

		gf_m2ts_remap_timestamps_for_pes(stream, stream->next_pck_flags, &stream->next_pck_dts, &stream->next_pck_cts, NULL);


		if (!(stream->next_pck_flags & GF_ESI_DATA_HAS_DTS))
			stream->next_pck_dts = stream->next_pck_cts;
	}
}


Bool gf_m2ts_stream_compute_pes_length(GF_M2TS_Mux_Stream *stream, u32 payload_length)
{
	assert(stream->pes_data_remain==0);
	stream->pes_data_len = stream->curr_pck.data_len - stream->pck_offset;

	stream->copy_from_next_packets = 0;
	/*if we have next payload ready, compute transmitted size*/
	if (stream->next_payload_size) {
		u32 pck_size = stream->curr_pck.data_len - stream->pck_offset;
		u32 ts_bytes = payload_length;

		/*finish this AU*/
		while (ts_bytes < pck_size) {
			ts_bytes += 184;
		}

		/*current AU started in PES, don't start new AU - if enough data
		still to be sent from current AU, don't send the complete AU
		in order to avoid padding*/
		if (stream->prevent_two_au_start_in_pes && !stream->pck_offset) {
			if (ts_bytes>184)
				ts_bytes -= 184;
			else
				ts_bytes = pck_size;
		}
		/*try to fit in part of the next AU*/
		else if (stream->next_payload_size) {
			/*how much more TS packets do we need to send next AU ?*/
			while (ts_bytes < pck_size + stream->next_payload_size) {
				ts_bytes += 184;
			}
			/*don't end next AU in next PES if we don't want to start 2 AUs in one PES
			if we don't have the N+2 AU size, don't try to pack it*/
			if ((stream->prevent_two_au_start_in_pes && (ts_bytes>pck_size + stream->next_payload_size))
			        || !stream->next_next_payload_size
			   ) {
				if (ts_bytes>184)
					ts_bytes -= 184;
				else
					ts_bytes = pck_size + stream->next_payload_size;
			}
		}

		/*that's how much bytes we copy from the following AUs*/
		if (ts_bytes >= pck_size) {
			stream->copy_from_next_packets = ts_bytes - pck_size;
		} else {
			u32 skipped = pck_size-ts_bytes;
			if (stream->pes_data_len > skipped)
				stream->pes_data_len -= skipped;
		}

		if (stream->min_bytes_copy_from_next && stream->copy_from_next_packets) {
			/*if we don't have enough space in the PES to store beginning of new AU, don't copy it and ask
			to recompute header (we might no longer have DTS/CTS signaled)*/
			if (stream->copy_from_next_packets < stream->min_bytes_copy_from_next) {
				stream->copy_from_next_packets = 0;
				stream->next_payload_size = 0;
				stream->next_pck_flags = 0;
				stream->next_pck_sap = 0;
				return GF_FALSE;
			}
			/*if what will remain after copying next AU is less than the minimum safety copy only copy next AU and
			realign n+2 AU start with PES*/
			if ((stream->copy_from_next_packets > stream->next_payload_size)
			        && (stream->copy_from_next_packets - stream->next_payload_size < stream->min_bytes_copy_from_next)
			   ) {
				stream->copy_from_next_packets = stream->next_payload_size;
			}
		}

		if (stream->pck_offset && !stream->copy_from_next_packets && stream->next_payload_size) {
			stream->copy_from_next_packets = 0;
			stream->next_payload_size = 0;
			stream->next_pck_flags = 0;
			stream->next_pck_sap = 0;
			return GF_FALSE;
		}

		if (stream->ifce->caps & GF_ESI_STREAM_IS_OVER) {
#if 0
			while (stream->copy_from_next_packets > stream->next_payload_size) {
				if (stream->copy_from_next_packets < 184) {
					stream->copy_from_next_packets = 0;
					break;
				}
				stream->copy_from_next_packets -= 184;
			}
#endif
			stream->pes_data_len += stream->next_payload_size;
		} else {
			stream->pes_data_len += stream->copy_from_next_packets;
		}
	}
	stream->pes_data_remain = stream->pes_data_len;
	return GF_TRUE;
}

static u32 gf_m2ts_stream_get_pes_header_length(GF_M2TS_Mux_Stream *stream)
{
	u32 hdr_len, flags;
	flags = stream->pck_offset ? stream->next_pck_flags : stream->curr_pck.flags;

	/*not done with the current pes*/
	if (stream->pes_data_remain) return 0;
	hdr_len = 9;
	/*signal timing only if AU start in the PES*/
	if ( flags & GF_ESI_DATA_AU_START) {
		if (flags & GF_ESI_DATA_HAS_CTS) hdr_len += 5;
		if (flags & GF_ESI_DATA_HAS_DTS) hdr_len += 5;
	}
	return hdr_len;
}

u32 gf_m2ts_stream_add_pes_header(GF_BitStream *bs, GF_M2TS_Mux_Stream *stream)
{
	u64 t, dts, cts;
	u32 pes_len;
	Bool use_pts, use_dts;

	gf_bs_write_int(bs,	0x1, 24);//packet start code
	gf_bs_write_u8(bs,	stream->mpeg2_stream_id);// stream id

	/*next AU start in current PES and current AU began in previous PES, use next AU timing*/
	if (stream->pck_offset && stream->copy_from_next_packets) {
		use_pts = (stream->next_pck_flags & GF_ESI_DATA_HAS_CTS) ? GF_TRUE : GF_FALSE;
		use_dts = (stream->next_pck_flags & GF_ESI_DATA_HAS_DTS) ? GF_TRUE : GF_FALSE;
		dts = stream->next_pck_dts;
		cts = stream->next_pck_cts;
	}
	/*we already sent the beginning of the AU*/
	else if (stream->pck_offset) {
		use_pts = use_dts = GF_FALSE;
		dts = cts = 0;
	} else {
		use_pts = (stream->curr_pck.flags & GF_ESI_DATA_HAS_CTS) ? GF_TRUE : GF_FALSE;
		use_dts = (stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS) ? GF_TRUE : GF_FALSE;
		dts = stream->curr_pck.dts;
		cts = stream->curr_pck.cts;
	}

	/*PES packet length: number of bytes in the PES packet following the last byte of the field "pes packet length"*/
	assert(stream->pes_data_len);
	pes_len = stream->pes_data_len + 3; // 3 = header size
	if (use_pts) pes_len += 5;
	if (use_dts) pes_len += 5;

	if (pes_len>0xFFFF) pes_len = 0;
	gf_bs_write_int(bs, pes_len, 16); // pes packet length

	gf_bs_write_int(bs, 0x2, 2); // reserved
	gf_bs_write_int(bs, 0x0, 2); // scrambling
	gf_bs_write_int(bs, 0x0, 1); // priority
	gf_bs_write_int(bs, stream->pck_offset ? 0 : 1, 1); // alignment indicator - we could also check start codes to see if we are aligned at slice/video packet level
	gf_bs_write_int(bs, 0x0, 1); // copyright
	gf_bs_write_int(bs, 0x0, 1); // original or copy

	gf_bs_write_int(bs, use_pts, 1);
	gf_bs_write_int(bs, use_dts, 1);
	gf_bs_write_int(bs, 0x0, 6); //6 flags = 0 (ESCR, ES_rate, DSM_trick, additional_copy, PES_CRC, PES_extension)

	gf_bs_write_int(bs, use_dts*5+use_pts*5, 8);

	if (use_pts) {
		gf_bs_write_int(bs, use_dts ? 0x3 : 0x2, 4); // reserved '0011' || '0010'
		t = ((cts >> 30) & 0x7);
		gf_bs_write_long_int(bs, t, 3);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = ((cts >> 15) & 0x7fff);
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = cts & 0x7fff;
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
	}

	if (use_dts) {
		gf_bs_write_int(bs, 0x1, 4); // reserved '0001'
		t = ((dts >> 30) & 0x7);
		gf_bs_write_long_int(bs, t, 3);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = ((dts >> 15) & 0x7fff);
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = dts & 0x7fff;
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Adding PES header at PCR "LLD" - has PTS %d ("LLU") - has DTS %d ("LLU") - Payload length %d\n", stream->pid, gf_m2ts_get_pcr(stream)/300, use_pts, cts, use_dts, dts, pes_len));

	return pes_len+4; // 4 = start code + stream_id
}

void gf_m2ts_mux_pes_get_next_packet(GF_M2TS_Mux_Stream *stream, char *packet)
{
	GF_BitStream *bs;
	Bool needs_pcr, first_pass;
	u32 adaptation_field_control, payload_length, payload_to_copy, padding_length, hdr_len, pos, copy_next;

	assert(stream->pid);
	bs = stream->program->mux->pck_bs;
	gf_bs_reassign_buffer(bs, packet, 188);

	if (stream->pcr_only_mode) {
		payload_length = 184 - 8;
		needs_pcr = GF_TRUE;
		adaptation_field_control = GF_M2TS_ADAPTATION_ONLY;
		hdr_len = 0;
	} else {
		hdr_len = gf_m2ts_stream_get_pes_header_length(stream);

		/*we may need two pass in case we first compute hdr len and TS payload size by considering
		we concatenate next au start in this PES but finally couldn't do it when computing PES len
		and AU alignment constraint of the stream*/
		first_pass = GF_TRUE;
		while (1) {
			if (hdr_len) {
				if (first_pass)
					gf_m2ts_stream_update_data_following(stream);
				hdr_len = gf_m2ts_stream_get_pes_header_length(stream);
			}

			adaptation_field_control = GF_M2TS_ADAPTATION_NONE;
			payload_length = 184 - hdr_len;
			needs_pcr = GF_FALSE;

			if (stream == stream->program->pcr) {
				if (hdr_len)
					needs_pcr = GF_TRUE;
				/*if we forced inserting PAT/PMT before new RAP, also insert PCR here*/
				if (stream->program->mux->force_pat_pmt_state == GF_SEG_BOUNDARY_FORCE_PCR) {
					stream->program->mux->force_pat_pmt_state = GF_SEG_BOUNDARY_NONE;
					needs_pcr = GF_TRUE;
				}

				if (!needs_pcr && (stream->program->mux->real_time || stream->program->mux->fixed_rate) ) {
					u64 clock;
					u32 diff;
					if (stream->program->mux->fixed_rate) {
						//check if PCR, if send at next packet, exceeds requested PCR update time
						clock = 1 + stream->program->mux->tot_pck_sent - stream->program->nb_pck_last_pcr;
						clock *= 1504*1000000;
						clock /= stream->program->mux->bit_rate;
						if (clock >= 500 + stream->program->mux->pcr_update_ms*1000) {
							needs_pcr = GF_TRUE;
						}
					}

					if (!needs_pcr && stream->program->mux->real_time) {
						clock = gf_sys_clock_high_res();
						diff = (u32) (clock - stream->program->sys_clock_at_last_pcr);

						if (diff >= 100 + stream->program->mux->pcr_update_ms*1000) {
							needs_pcr = GF_TRUE;
						}
					}
				}
			}

			if (needs_pcr) {
				/*AF headers + PCR*/
				payload_length -= 8;
				adaptation_field_control = GF_M2TS_ADAPTATION_AND_PAYLOAD;
			}
			//af descriptors are only inserted at the start of the pes for the time being
			if (hdr_len && stream->curr_pck.mpeg2_af_descriptors) {
				if (adaptation_field_control == GF_M2TS_ADAPTATION_NONE) {
					payload_length -= 2; //AF header but no PCR
					adaptation_field_control = GF_M2TS_ADAPTATION_AND_PAYLOAD;
				}
				payload_length -= 2 + stream->curr_pck.mpeg2_af_descriptors_size; //AF extension field and AF descriptor
			}

			if (hdr_len) {
				assert(!stream->pes_data_remain);
				if (! gf_m2ts_stream_compute_pes_length(stream, payload_length)) {
					first_pass = GF_FALSE;
					continue;
				}

				assert(stream->pes_data_remain==stream->pes_data_len);
			}
			break;
		}
	}

	copy_next = stream->copy_from_next_packets;
	payload_to_copy = stream->curr_pck.data_len - stream->pck_offset;
	/*end of PES packet*/
	if (payload_to_copy >= stream->pes_data_remain) {
		payload_to_copy = stream->pes_data_remain;
		copy_next = 0;
	}


	/*packet exceed payload length*/
	if (payload_to_copy >= payload_length) {
		padding_length = 0;
		payload_to_copy = payload_length;
	}
	/*packet + next packet exceed payload length*/
	else if (payload_to_copy + copy_next >= payload_length) {
		padding_length = 0;
	}
	/*packet + next packet less than payload length - pad */
	else {
		/*AF headers*/
		if (!needs_pcr) {
			if (adaptation_field_control == GF_M2TS_ADAPTATION_NONE) {
				payload_length -= 2;
				adaptation_field_control = GF_M2TS_ADAPTATION_AND_PAYLOAD;
			}
		}
		/*cannot add adaptation field for this TS packet with this payload, we need to split in 2 TS packets*/
		if (payload_length < payload_to_copy + copy_next) {
			padding_length = 10;
			payload_length -= padding_length;
			if (payload_to_copy > payload_length)
				payload_to_copy = payload_length;
		} else {
			padding_length = payload_length - payload_to_copy - copy_next;
			payload_length -= padding_length;
		}
	}

#ifndef GPAC_DISABLE_LOG
	if (hdr_len && gf_log_tool_level_on(GF_LOG_CONTAINER, GF_LOG_DEBUG) ) {
		u64 pcr = (s64) gf_m2ts_get_pcr(stream)/300;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Start sending PES: PID %d - %d bytes - DTS "LLD" PCR "LLD" (diff %d) - stream time %d:%09d - mux time %d:%09d (%d us ahead of mux time)\n",
		                                        stream->pid, stream->curr_pck.data_len, stream->curr_pck.dts, pcr, (s64) stream->curr_pck.dts - (s64) pcr,
		                                        stream->time.sec, stream->time.nanosec, stream->program->mux->time.sec, stream->program->mux->time.nanosec,
		                                        gf_m2ts_time_diff_us(&stream->program->mux->time, &stream->time)
		                                       ));
	}
#endif

	//CC field shall not be incremented for if adaptation field only, rewind counter
	if (adaptation_field_control == GF_M2TS_ADAPTATION_ONLY) {
		if (!stream->continuity_counter) stream->continuity_counter=15;
		else stream->continuity_counter--;
	}

	gf_bs_write_int(bs,	0x47, 8); // sync byte
	gf_bs_write_int(bs,	0, 1);    // error indicator
	gf_bs_write_int(bs,	hdr_len ? 1 : 0, 1); // start ind
	gf_bs_write_int(bs,	0, 1);	  // transport priority
	gf_bs_write_int(bs,	stream->pid, 13); // pid
	gf_bs_write_int(bs,	0, 2);    // scrambling
	gf_bs_write_int(bs,	adaptation_field_control, 2);    // we do not use adaptation field for sections
	gf_bs_write_int(bs,	stream->continuity_counter, 4);   // continuity counter

	if (stream->continuity_counter < 15) stream->continuity_counter++;
	else stream->continuity_counter=0;

	if (adaptation_field_control != GF_M2TS_ADAPTATION_NONE) {
		Bool is_rap = GF_FALSE;
		u64 pcr = 0;
		if (needs_pcr) {
			u64 now = gf_sys_clock_high_res();
			pcr = gf_m2ts_get_pcr(stream);

			if (stream->program->mux->real_time || stream->program->mux->fixed_rate) {
				u64 clock;
				clock = stream->program->mux->tot_pck_sent - stream->program->nb_pck_last_pcr;
				clock *= 1504000000;
				clock /= stream->program->mux->bit_rate;

				//allow 2 ms drift
				if (clock > 2000 + stream->program->mux->pcr_update_ms*1000) {
					GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] PCR sent %d us later than requested PCR send rate %d ms\n", (s32) (clock - stream->program->mux->pcr_update_ms*1000), stream->program->mux->pcr_update_ms ));
				}

				if (stream->program->mux->real_time) {
					u32 diff = (s32) (now - stream->program->sys_clock_at_last_pcr);
					//since we currently only send the PCR when an AU is sent, it may happen that we exceed PCR the refresh rate depending in the target bitrate and frame rate.
					//we only throw a warning when twiice the PCR refresh is exceeded
					if (diff > 5000 + 2*stream->program->mux->pcr_update_ms*1000 ) {
						GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sending PCR %d us too late (PCR send rate %d ms)\n", (u32) (diff - stream->program->mux->pcr_update_ms*1000), stream->program->mux->pcr_update_ms ));
					}
				}
			}

			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Inserted PCR "LLD" (%d @90kHz) at mux time %d:%09d\n", pcr, (u32) (pcr/300), stream->program->mux->time.sec, stream->program->mux->time.nanosec ));
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] PCR diff to STB in ms %d - sys clock diff in ms %d - DTS diff %d\n", (u32) (pcr - stream->program->last_pcr) / 27000, now - stream->program->sys_clock_at_last_pcr, (stream->curr_pck.dts - stream->program->last_dts)/90));

			stream->program->sys_clock_at_last_pcr = now;
			stream->program->last_dts = stream->curr_pck.dts;
			stream->program->nb_pck_last_pcr = stream->program->mux->tot_pck_sent;
		}
		is_rap = (hdr_len && (stream->curr_pck.sap_type) ) ? GF_TRUE : GF_FALSE;
		gf_m2ts_add_adaptation(stream->program, bs, stream->pid, needs_pcr, pcr, is_rap, padding_length, hdr_len ? stream->curr_pck.mpeg2_af_descriptors : NULL, hdr_len ? stream->curr_pck.mpeg2_af_descriptors_size : 0, stream->set_initial_disc);
		stream->set_initial_disc = GF_FALSE;

		if (stream->curr_pck.mpeg2_af_descriptors) {
			gf_free(stream->curr_pck.mpeg2_af_descriptors);
			stream->curr_pck.mpeg2_af_descriptors = NULL;
			stream->curr_pck.mpeg2_af_descriptors_size = 0;
		}

		if (padding_length)
			stream->program->mux->tot_pes_pad_bytes += padding_length;
	}
	stream->pck_sap_type = 0;
	stream->pck_sap_time = 0;
	if (hdr_len) {
		gf_m2ts_stream_add_pes_header(bs, stream);
		if (stream->curr_pck.sap_type) {
			stream->pck_sap_type = 1;
			stream->pck_sap_time = stream->curr_pck.cts;
		}
	}

	pos = (u32) gf_bs_get_position(bs);

	if (adaptation_field_control == GF_M2TS_ADAPTATION_ONLY) {
		return;
	}

	assert(stream->curr_pck.data_len - stream->pck_offset >= payload_to_copy);
	memcpy(packet+pos, stream->curr_pck.data + stream->pck_offset, payload_to_copy);
	stream->pck_offset += payload_to_copy;
	assert(stream->pes_data_remain >= payload_to_copy);
	stream->pes_data_remain -= payload_to_copy;

	/*update stream time, including headers*/
	gf_m2ts_time_inc(&stream->time, 1504/*188*8*/, stream->program->mux->bit_rate);

	if (stream->pck_offset == stream->curr_pck.data_len) {
		u64 pcr = gf_m2ts_get_pcr(stream)/300;
		if (stream->program->mux->real_time && !stream->program->mux->fixed_rate && gf_m2ts_time_less(&stream->time, &stream->program->mux->time) ) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sent PES TOO LATE: PID %d - DTS "LLD" - PCR "LLD" - stream time %d:%09d - mux time %d:%09d - current mux rate %d\n",
			                                       stream->pid, stream->curr_pck.dts, pcr,
			                                       stream->time.sec, stream->time.nanosec, stream->program->mux->time.sec, stream->program->mux->time.nanosec,
			                                       stream->program->mux->bit_rate
			                                      ));
		} else if (stream->curr_pck.dts < pcr) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sent PES %d us TOO LATE: PID %d - DTS "LLD" - size %d\n\tPCR "LLD" - stream time %d:%09d - mux time %d:%09d \n",
			        (pcr - stream->curr_pck.dts)*100/9, stream->pid, stream->curr_pck.dts, stream->curr_pck.data_len, pcr,
			        stream->time.sec, stream->time.nanosec, stream->program->mux->time.sec, stream->program->mux->time.nanosec
			                                         ));
		} else if (stream->curr_pck.cts < pcr) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sent PES %d us TOO LATE: PID %d - DTS "LLD" - size %d\n\tPCR "LLD" - stream time %d:%09d - mux time %d:%09d \n",
			        pcr - stream->curr_pck.dts, stream->pid, stream->curr_pck.dts, stream->curr_pck.data_len, pcr,
			        stream->time.sec, stream->time.nanosec, stream->program->mux->time.sec, stream->program->mux->time.nanosec
			                                         ));
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sent PES: PID %d - DTS "LLD" - PCR "LLD" - stream time %d:%09d - mux time %d:%09d \n",
			                                       stream->pid, stream->curr_pck.dts, pcr,
			                                       stream->time.sec, stream->time.nanosec, stream->program->mux->time.sec, stream->program->mux->time.nanosec
			                                      ));

		}

		/*PES has been sent, discard internal buffer*/
		if (stream->discard_data) gf_free(stream->curr_pck.data);
		stream->curr_pck.data = NULL;
		stream->curr_pck.data_len = 0;
		if (stream->curr_pck.mpeg2_af_descriptors) gf_free(stream->curr_pck.mpeg2_af_descriptors);
		stream->curr_pck.mpeg2_af_descriptors = NULL;
		stream->curr_pck.mpeg2_af_descriptors_size = 0;
		stream->pck_offset = 0;

//#ifndef GPAC_DISABLE_LOG
#if 0
		if (gf_m2ts_time_less(&stream->program->mux->time, &stream->time) ) {
			s32 drift = gf_m2ts_time_diff_us(&stream->program->mux->time, &stream->time);
			GF_LOG( (drift>1000) ? GF_LOG_WARNING : GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] PES PID %d sent %d us too late\n", stream->pid, drift) );
		}
#endif

		/*copy over from next PES to fill this TS packet*/
		if (stream->copy_from_next_packets) {
			u32 copy_next;
			pos += payload_to_copy;
			copy_next = payload_length - payload_to_copy;
			/*we might need a more than one*/
			while (stream->pes_data_remain) {
				u32 remain = 0;
				Bool res = stream->process(stream->program->mux, stream);
				if (!res) {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Not enough data to fill current PES (PID %d) - filling with 0xFF\n", stream->pid) );
					memset(packet+pos, 0xFF, copy_next);

					if (stream->copy_from_next_packets > copy_next) {
						stream->copy_from_next_packets -= copy_next;
					} else {
						stream->copy_from_next_packets = 0;
					}
					break;
				}
				if (copy_next > stream->curr_pck.data_len) {
					remain = copy_next - stream->curr_pck.data_len;
					copy_next = stream->curr_pck.data_len;
				}

				memcpy(packet+pos, stream->curr_pck.data + stream->pck_offset, copy_next);
				stream->pck_offset += copy_next;
				assert(stream->pes_data_remain >= copy_next);
				stream->pes_data_remain -= copy_next;

				if (stream->copy_from_next_packets > copy_next) {
					stream->copy_from_next_packets -= copy_next;
				} else {
					stream->copy_from_next_packets = 0;
				}

				if (stream->pck_offset == stream->curr_pck.data_len) {
					assert(!remain || (remain>=stream->min_bytes_copy_from_next));
					/*PES has been sent, discard internal buffer*/
					if (stream->discard_data) gf_free(stream->curr_pck.data);
					stream->curr_pck.data = NULL;
					stream->curr_pck.data_len = 0;
					if (stream->curr_pck.mpeg2_af_descriptors) gf_free(stream->curr_pck.mpeg2_af_descriptors);
					stream->curr_pck.mpeg2_af_descriptors = NULL;
					stream->curr_pck.mpeg2_af_descriptors_size = 0;
					stream->pck_offset = 0;
				}
				if (!remain) break;
				pos += copy_next;
				copy_next = remain;
			}
		}
		else if (stream->program->mux->force_pat_pmt_state==GF_SEG_BOUNDARY_START) {
			stream->program->mux->force_pat_pmt_state = GF_SEG_BOUNDARY_FORCE_PAT;
			stream->program->mux->force_pat = GF_TRUE;
		}
	}
	stream->bytes_since_last_time += 188;
}


GF_M2TS_Mux_Stream *gf_m2ts_stream_new(u32 pid) {
	GF_M2TS_Mux_Stream *stream;

	GF_SAFEALLOC(stream, GF_M2TS_Mux_Stream);
	if (!stream) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: fail to allocate\n", pid));
		return NULL;
	}
	stream->pid = pid;
	stream->process = gf_m2ts_stream_process_pes;

	return stream;
}

GF_Err gf_m2ts_output_ctrl(GF_ESInterface *_self, u32 ctrl_type, void *param)
{
	GF_ESIPacket *esi_pck;

	GF_M2TS_Mux_Stream *stream = (GF_M2TS_Mux_Stream *)_self->output_udta;
	switch (ctrl_type) {
	case GF_ESI_OUTPUT_DATA_DISPATCH:
		esi_pck = (GF_ESIPacket *)param;

		if (stream->force_new || (esi_pck->flags & GF_ESI_DATA_AU_START)) {
			if (stream->pck_reassembler) {
				if (stream->mx) gf_mx_p(stream->mx);
				if (!stream->pck_first) {
					stream->pck_first = stream->pck_last = stream->pck_reassembler;
				} else {
					stream->pck_last->next = stream->pck_reassembler;
					stream->pck_last = stream->pck_reassembler;
				}
				if (stream->mx) gf_mx_v(stream->mx);
				stream->pck_reassembler = NULL;
			}
		}
		if (!stream->pck_reassembler) {
			GF_SAFEALLOC(stream->pck_reassembler, GF_M2TS_Packet);
			if (!stream->pck_reassembler) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: fail to allocate packet reassembler\n", stream->pid));
				return GF_OUT_OF_MEM;
			}

			stream->pck_reassembler->cts = esi_pck->cts;
			stream->pck_reassembler->dts = esi_pck->dts;
			stream->pck_reassembler->duration = esi_pck->duration;
			if (esi_pck->mpeg2_af_descriptors) {
				stream->pck_reassembler->mpeg2_af_descriptors = (char*)gf_realloc(stream->pck_reassembler->mpeg2_af_descriptors, sizeof(u8)* (stream->pck_reassembler->mpeg2_af_descriptors_size + esi_pck->mpeg2_af_descriptors_size) );
				memcpy(stream->pck_reassembler->mpeg2_af_descriptors + stream->pck_reassembler->mpeg2_af_descriptors_size, esi_pck->mpeg2_af_descriptors, sizeof(u8)* esi_pck->mpeg2_af_descriptors_size );
				stream->pck_reassembler->mpeg2_af_descriptors_size += esi_pck->mpeg2_af_descriptors_size;
			}
		}

		stream->force_new = esi_pck->flags & GF_ESI_DATA_AU_END ? GF_TRUE : GF_FALSE;

		stream->pck_reassembler->data = (char*)gf_realloc(stream->pck_reassembler->data , sizeof(char)*(stream->pck_reassembler->data_len+esi_pck->data_len) );
		memcpy(stream->pck_reassembler->data + stream->pck_reassembler->data_len, esi_pck->data, esi_pck->data_len);
		stream->pck_reassembler->data_len += esi_pck->data_len;

		stream->pck_reassembler->flags |= esi_pck->flags;
		if (esi_pck->sap_type) stream->pck_reassembler->sap_type = esi_pck->sap_type;
		if (stream->force_new) {
			if (stream->mx) gf_mx_p(stream->mx);
			if (!stream->pck_first) {
				stream->pck_first = stream->pck_last = stream->pck_reassembler;
			} else {
				stream->pck_last->next = stream->pck_reassembler;
				stream->pck_last = stream->pck_reassembler;
			}
			if (stream->mx) gf_mx_v(stream->mx);
			stream->pck_reassembler = NULL;
		}
		break;
	}
	return GF_OK;
}

static void gf_m2ts_stream_set_default_slconfig(GF_M2TS_Mux_Stream *stream)
{
	if (!stream->ifce->sl_config) {
		stream->ifce->sl_config = (GF_SLConfig *)gf_odf_desc_new(GF_ODF_SLC_TAG);
		stream->ifce->sl_config->useAccessUnitStartFlag = 1;
		stream->ifce->sl_config->useAccessUnitEndFlag = 1;
		stream->ifce->sl_config->useRandomAccessPointFlag = 1;
		stream->ifce->sl_config->useTimestampsFlag = 1;
	}
}

static s32 gf_m2ts_stream_index(GF_M2TS_Mux_Program *program, u32 pid, u32 stream_id)
{
	s32 i=0;
	GF_M2TS_Mux_Stream *st = program->streams;
	while (st) {
		if (pid && (st->pid == pid))
			return i;
		if (stream_id && (st->ifce->stream_id == stream_id))
			return i;
		st = st->next;
		i++;
	}
	return -1;
}


static void gf_m2ts_stream_add_hierarchy_descriptor(GF_M2TS_Mux_Stream *stream)
{
	GF_M2TSDescriptor *desc;
	GF_BitStream *bs;
	u32 data_len;
	if (!stream || !stream->program || !stream->program->pmt) return;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	/*reserved*/
	gf_bs_write_int(bs, 1, 1);
	/*temporal_scalability_flag: use the reserved value*/
	gf_bs_write_int(bs, 1, 1);
	/*spatial_scalability_flag: use the reserved value*/
	gf_bs_write_int(bs, 1, 1);
	/*quality_scalability_flag: use the reserved value*/
	gf_bs_write_int(bs, 1, 1);
	/*hierarchy_type: use the rerserved value*/
	gf_bs_write_int(bs, 0, 4);
	/*reserved*/
	gf_bs_write_int(bs, 3, 2);
	/*hierarchy_layer_index*/
	gf_bs_write_int(bs, gf_m2ts_stream_index(stream->program, stream->pid, 0), 6);
	/*tref_present_flag = 1 : NOT PRESENT*/
	gf_bs_write_int(bs, 1, 1);
	/*reserved*/
	gf_bs_write_int(bs, 1, 1);
	/*hierarchy_embedded_layer_index*/
	gf_bs_write_int(bs, gf_m2ts_stream_index(stream->program, 0, stream->ifce->depends_on_stream), 6);
	/*reserved*/
	gf_bs_write_int(bs, 3, 2);
	/*hierarchy_channel*/
	gf_bs_write_int(bs, stream->ifce->stream_id, 6);

	GF_SAFEALLOC(desc, GF_M2TSDescriptor);
	if (!desc) return;

	desc->tag = (u8) GF_M2TS_HIERARCHY_DESCRIPTOR;
	gf_bs_get_content(bs, &desc->data, &data_len);
	gf_bs_del(bs);
	desc->data_len = (u8) data_len;
	gf_list_add(stream->loop_descriptors, desc);
}

static void gf_m2ts_stream_add_metadata_pointer_descriptor(GF_M2TS_Mux_Program *program)
{
	GF_M2TSDescriptor *desc;
	GF_BitStream *bs;
	u32 data_len;
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u16(bs, 0xFFFF);
	gf_bs_write_u32(bs, GF_M2TS_META_ID3);
	gf_bs_write_u8(bs, 0xFF);
	gf_bs_write_u32(bs, GF_M2TS_META_ID3);
	gf_bs_write_u8(bs, 0); /* service id */
	gf_bs_write_int(bs, 0, 1); /* locator */
	gf_bs_write_int(bs, 0, 2); /* carriage flags */
	gf_bs_write_int(bs, 0x1F, 5); /* reserved */
	gf_bs_write_u16(bs, program->number);
	GF_SAFEALLOC(desc, GF_M2TSDescriptor);
	if (!desc) return;

	desc->tag = (u8) GF_M2TS_METADATA_POINTER_DESCRIPTOR;
	gf_bs_get_content(bs, &desc->data, &data_len);
	gf_bs_del(bs);
	desc->data_len = (u8) data_len;
	gf_list_add(program->loop_descriptors, desc);
}

static void gf_m2ts_stream_add_metadata_descriptor(GF_M2TS_Mux_Stream *stream)
{
	GF_M2TSDescriptor *desc;
	GF_BitStream *bs;
	u32 data_len;
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_bs_write_u16(bs, 0xFFFF);
	gf_bs_write_u32(bs, GF_M2TS_META_ID3);
	gf_bs_write_u8(bs, 0xFF);
	gf_bs_write_u32(bs, GF_M2TS_META_ID3);
	gf_bs_write_u8(bs, 0); /* service id */
	gf_bs_write_int(bs, 0, 3); /* decoder config flags */
	gf_bs_write_int(bs, 0, 1); /* dsmcc flag */
	gf_bs_write_int(bs, 0xF, 4); /* reserved */
	GF_SAFEALLOC(desc, GF_M2TSDescriptor);
	if (!desc) return;

	desc->tag = (u8) GF_M2TS_METADATA_DESCRIPTOR;
	gf_bs_get_content(bs, &desc->data, &data_len);
	gf_bs_del(bs);
	desc->data_len = (u8) data_len;
	gf_list_add(stream->loop_descriptors, desc);
}

GF_EXPORT
u32 gf_m2ts_mux_program_get_pmt_pid(GF_M2TS_Mux_Program *prog)
{
	return prog->pmt ? prog->pmt->pid : 0;
}

GF_EXPORT
u32 gf_m2ts_mux_program_get_pcr_pid(GF_M2TS_Mux_Program *prog)
{
	return prog->pcr ? prog->pcr->pid : 0;
}

GF_EXPORT
u32 gf_m2ts_mux_program_get_stream_count(GF_M2TS_Mux_Program *prog)
{
	GF_M2TS_Mux_Stream *stream = prog->streams;
	u32 count=0;
	while (stream) {
		count++;
		stream = stream->next;
	}
	return count;
}

GF_EXPORT
GF_M2TS_Mux_Stream *gf_m2ts_program_stream_add(GF_M2TS_Mux_Program *program, struct __elementary_stream_ifce *ifce, u32 pid, Bool is_pcr, Bool force_pes, Bool needs_mutex)
{
	GF_M2TS_Mux_Stream *stream, *st;

	stream = gf_m2ts_stream_new(pid);
	stream->ifce = ifce;
	stream->pid = pid;
	stream->program = program;
	if (is_pcr) program->pcr = stream;
	stream->loop_descriptors = gf_list_new();
	stream->set_initial_disc = program->initial_disc_set;

	if (program->streams) {
		/*if PCR keep stream at the beginning*/
		if (is_pcr) {
			stream->next = program->streams;
			program->streams = stream;
		} else {
			st = program->streams;
			while (st->next) st = st->next;
			st->next = stream;
		}
	} else {
		program->streams = stream;
	}
	if (program->pmt) program->pmt->table_needs_update = GF_TRUE;
	stream->bit_rate = ifce->bit_rate;
	stream->scheduling_priority = 1;

	stream->force_single_au = (stream->program->mux->au_pes_mode == GF_M2TS_PACK_ALL) ? GF_FALSE : GF_TRUE;

	switch (ifce->stream_type) {
	case GF_STREAM_VISUAL:
		/*just pick first valid stream_id in visual range*/
		stream->mpeg2_stream_id = 0xE0;
		/*for video streams, prevent sending two frames start in one PES. This will
		introduce more overhead at very low bitrates where such cases happen, but will ensure proper timing
		of each frame*/
		stream->prevent_two_au_start_in_pes = GF_TRUE;
		switch (ifce->codecid) {
		case GF_CODECID_MPEG4_PART2:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG4;
			break;
		case GF_CODECID_AVC:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_H264;
			/*make sure we send AU delim NALU in same PES as first VCL NAL: 6 bytes (start code + 1 nal hdr + AU delim)
			+ 4 byte start code + first nal header*/
			stream->min_bytes_copy_from_next = 11;
			break;
		case GF_CODECID_SVC:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_SVC;
			/*make sure we send AU delim NALU in same PES as first VCL NAL: 6 bytes (start code + 1 nal hdr + AU delim)
			+ 4 byte start code + first nal header*/
			stream->min_bytes_copy_from_next = 11;
			gf_m2ts_stream_add_hierarchy_descriptor(stream);
			break;
		case GF_CODECID_HEVC:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_HEVC;
			/*make sure we send AU delim NALU in same PES as first VCL NAL: 7 bytes (4 start code + 2 nal header + 1 AU delim)
			+ 4 byte start code + first nal header*/
			stream->min_bytes_copy_from_next = 12;
			break;
		case GF_CODECID_LHVC:
			//FIXME - we need to check scalability type to see if we have MHVC, for now only use SHVC
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_SHVC;
			/*make sure we send AU delim NALU in same PES as first VCL NAL: 7 bytes (4 start code + 2 nal header + 1 AU delim)
			+ 4 byte start code + first nal header*/
			stream->min_bytes_copy_from_next = 12;
			gf_m2ts_stream_add_hierarchy_descriptor(stream);
			//force by default with SHVC since we don't have any delimiter / layer yet
			stream->force_single_au = GF_TRUE;
			break;
		case GF_CODECID_MPEG1:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG1;
			break;
		case GF_CODECID_MPEG2_SIMPLE:
		case GF_CODECID_MPEG2_MAIN:
		case GF_CODECID_MPEG2_SNR:
		case GF_CODECID_MPEG2_SPATIAL:
		case GF_CODECID_MPEG2_HIGH:
		case GF_CODECID_MPEG2_422:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG2;
			break;
		/*JPEG/PNG carried in MPEG-4 PES*/
		case GF_CODECID_JPEG:
		case GF_CODECID_PNG:
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
			stream->force_single_au = GF_TRUE;
			stream->mpeg2_stream_id = 0xFA;
			gf_m2ts_stream_set_default_slconfig(stream);
			break;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] Unsupported mpeg2-ts video type for codec %s, signaling as PES private using codec 4CC in registration descriptor\n", gf_codecid_name(ifce->codecid) ));

			stream->mpeg2_stream_type = GF_M2TS_PRIVATE_DATA;
			stream->force_single_au = GF_TRUE;
			stream->force_reg_desc = GF_TRUE;
			break;
		}
		break;
	case GF_STREAM_AUDIO:
		/*just pick first valid stream_id in audio range*/
		stream->mpeg2_stream_id = 0xC0;
		//override default packing for audio
		stream->force_single_au = (stream->program->mux->au_pes_mode == GF_M2TS_PACK_NONE) ? GF_TRUE : GF_FALSE;
		switch (ifce->codecid) {
		case GF_CODECID_MPEG_AUDIO:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_MPEG1;
			break;
		case GF_CODECID_MPEG2_PART3:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_MPEG2;
			break;
		case GF_CODECID_AAC_MPEG4:
		case GF_CODECID_AAC_MPEG2_MP:
		case GF_CODECID_AAC_MPEG2_LCP:
		case GF_CODECID_AAC_MPEG2_SSRP:
			if (ifce->caps & GF_ESI_AAC_USE_LATM)
				stream->mpeg2_stream_type = GF_M2TS_AUDIO_LATM_AAC;
			else
				stream->mpeg2_stream_type = GF_M2TS_AUDIO_AAC;

			if (!ifce->repeat_rate) ifce->repeat_rate = 500;
			break;
		case GF_CODECID_AC3:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_AC3;
			break;
		case GF_CODECID_EAC3:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_EC3;
			break;
		default:
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] Unsupported mpeg2-ts audio type for codec %s, signaling as PES private using codec 4CC in registration descriptor\n", gf_codecid_name(ifce->codecid) ));
			stream->mpeg2_stream_type = GF_M2TS_PRIVATE_DATA;
			stream->force_single_au = GF_TRUE;
			stream->force_reg_desc = GF_TRUE;
			break;
		}
		break;
	case GF_STREAM_OD:
		/*highest priority for OD streams as they are needed to process other streams*/
		stream->scheduling_priority = 20;
		stream->mpeg2_stream_id = 0xFA;
		stream->table_id = GF_M2TS_TABLE_ID_MPEG4_OD;
		gf_m2ts_stream_set_default_slconfig(stream);
		if (force_pes) {
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
			stream->force_single_au = GF_TRUE;
		} else {
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_SECTIONS;
		}
		break;
	case GF_STREAM_SCENE:
		stream->mpeg2_stream_id = 0xFA;
		stream->table_id = GF_M2TS_TABLE_ID_MPEG4_BIFS;
		gf_m2ts_stream_set_default_slconfig(stream);

		if (force_pes) {
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
			stream->force_single_au = GF_TRUE;
		} else {
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_SECTIONS;
		}
		break;
	case GF_STREAM_TEXT:
		stream->mpeg2_stream_id = 0xBD;
		stream->mpeg2_stream_type = GF_M2TS_METADATA_PES;
		gf_m2ts_stream_add_metadata_pointer_descriptor(stream->program);
		gf_m2ts_stream_add_metadata_descriptor(stream);
		break;
	default:
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] Unsupported codec %s, signaling as raw data\n", gf_codecid_name(ifce->codecid) ));
		stream->mpeg2_stream_id = 0xBD;
		stream->mpeg2_stream_type = GF_M2TS_METADATA_PES;
		break;
	}

	if (! (ifce->caps & GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS)) {
		/*override signaling for all streams except BIFS/OD, to use MPEG-4 PES*/
		if (program->mpeg4_signaling==GF_M2TS_MPEG4_SIGNALING_FULL) {
			if (stream->mpeg2_stream_type != GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
				stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
				stream->force_single_au = GF_TRUE;
				stream->mpeg2_stream_id = 0xFA;/*ISO/IEC14496-1_SL-packetized_stream*/
				gf_m2ts_stream_set_default_slconfig(stream);
			}
		}
	}

	stream->ifce->output_ctrl = gf_m2ts_output_ctrl;
	stream->ifce->output_udta = stream;
	if (needs_mutex)
		stream->mx = gf_mx_new("M2TS PID");

	if (ifce->timescale != 90000) {
		stream->ts_scale.num = 90000;
		stream->ts_scale.den = ifce->timescale;
	}
	return stream;
}

#if 0 //unused
GF_Err gf_m2ts_program_stream_update_ts_scale(GF_ESInterface *_self, u32 time_scale)
{
	GF_M2TS_Mux_Stream *stream = (GF_M2TS_Mux_Stream *)_self->output_udta;
	if (!stream || !time_scale)
		return GF_BAD_PARAM;

	stream->ts_scale.num = 90000;
	stream->ts_scale.den = time_scale;
	return GF_OK;
}
#endif

GF_EXPORT
GF_M2TS_Mux_Program *gf_m2ts_mux_program_find(GF_M2TS_Mux *muxer, u32 program_number)
{
	GF_M2TS_Mux_Program *program = muxer->programs;
	while (program) {
		if (program->number == program_number) return program;
		program = program->next;
	}
	return NULL;
}


GF_EXPORT
u32 gf_m2ts_mux_program_count(GF_M2TS_Mux *muxer)
{
	u32 num=0;
	GF_M2TS_Mux_Program *program = muxer->programs;
	while (program) {
		num++;
		program = program->next;
	}
	return num;
}

GF_EXPORT
GF_M2TS_Mux_Program *gf_m2ts_mux_program_add(GF_M2TS_Mux *muxer, u32 program_number, u32 pmt_pid, u32 pmt_refresh_rate, u32 pcr_offset, Bool mpeg4_signaling, u32 pmt_version, Bool initial_disc)
{
	GF_M2TS_Mux_Program *program;

	GF_SAFEALLOC(program, GF_M2TS_Mux_Program);
	if (!program) return NULL;

	program->mux = muxer;
	program->mpeg4_signaling = mpeg4_signaling;
	program->pcr_offset = pcr_offset;
	program->loop_descriptors = gf_list_new();

	program->number = program_number;
	if (muxer->programs) {
		GF_M2TS_Mux_Program *p = muxer->programs;
		while (p->next) p = p->next;
		p->next = program;
	} else {
		muxer->programs = program;
	}
	program->pmt = gf_m2ts_stream_new(pmt_pid);
	program->pmt->program = program;
	program->pmt->table_needs_update = GF_TRUE;
	program->pmt->initial_version_number = pmt_version;
	program->initial_disc_set = initial_disc;
	program->pmt->set_initial_disc = initial_disc;
	muxer->pat->table_needs_update = GF_TRUE;
	program->pmt->process = gf_m2ts_stream_process_pmt;
	program->pmt->refresh_rate_ms = pmt_refresh_rate ? pmt_refresh_rate : (u32) -1;
	return program;
}

GF_EXPORT
void gf_m2ts_mux_program_set_name(GF_M2TS_Mux_Program *program, const char *program_name, const char *provider_name)
{
	if (program->name) gf_free(program->name);
	program->name = program_name ? gf_strdup(program_name) : NULL;

	if (program->provider) gf_free(program->provider);
	program->provider = provider_name ? gf_strdup(provider_name) : NULL;

	if (program->mux->sdt) program->mux->sdt->table_needs_update = GF_TRUE;
}

GF_EXPORT
GF_M2TS_Mux *gf_m2ts_mux_new(u32 mux_rate, u32 pat_refresh_rate, Bool real_time)
{
	GF_M2TS_Mux *muxer;
	GF_SAFEALLOC(muxer, GF_M2TS_Mux);
	if (!muxer) return NULL;

	muxer->pat = gf_m2ts_stream_new(GF_M2TS_PID_PAT);
	if (!muxer->pat) {
		gf_free(muxer);
		return NULL;
	}
	muxer->pat->process = gf_m2ts_stream_process_pat;
	muxer->pat->refresh_rate_ms = pat_refresh_rate ? pat_refresh_rate : (u32) -1;
	muxer->real_time = real_time;
	muxer->bit_rate = mux_rate;
	muxer->init_pcr_value = 0;
	if (mux_rate) muxer->fixed_rate = GF_TRUE;

	/*format NULL packet*/
	muxer->pck_bs = gf_bs_new(muxer->null_pck, 188, GF_BITSTREAM_WRITE);
	gf_bs_write_int(muxer->pck_bs,	0x47, 8);
	gf_bs_write_int(muxer->pck_bs,	0, 1);
	gf_bs_write_int(muxer->pck_bs,	0, 1);
	gf_bs_write_int(muxer->pck_bs,	0, 1);
	gf_bs_write_int(muxer->pck_bs,	0x1FFF, 13);
	gf_bs_write_int(muxer->pck_bs,	0, 2);
	gf_bs_write_int(muxer->pck_bs,	1, 2);
	gf_bs_write_int(muxer->pck_bs,	0, 4);

	gf_rand_init(GF_FALSE);
	muxer->pcr_update_ms = 100;
	return muxer;
}

GF_EXPORT
void gf_m2ts_mux_enable_sdt(GF_M2TS_Mux *mux, u32 refresh_rate_ms)
{
	if (!mux->sdt) {
		mux->sdt = gf_m2ts_stream_new(GF_M2TS_PID_SDT_BAT_ST);
		mux->sdt->process = gf_m2ts_stream_process_sdt;
		mux->sdt->refresh_rate_ms = refresh_rate_ms;
	}
	mux->sdt->table_needs_update = GF_TRUE;
	return;
}


GF_EXPORT
void gf_m2ts_mux_set_pcr_max_interval(GF_M2TS_Mux *muxer, u32 pcr_update_ms)
{
	if (muxer && (pcr_update_ms<=100)) muxer->pcr_update_ms = pcr_update_ms;
}

void gf_m2ts_mux_stream_del(GF_M2TS_Mux_Stream *st)
{
	while (st->tables) {
		GF_M2TS_Mux_Table *tab = st->tables->next;
		while (st->tables->section) {
			GF_M2TS_Mux_Section *sec = st->tables->section->next;
			gf_free(st->tables->section->data);
			gf_free(st->tables->section);
			st->tables->section = sec;
		}
		gf_free(st->tables);
		st->tables = tab;
	}
	while (st->pck_first) {
		GF_M2TS_Packet *curr_pck = st->pck_first;
		st->pck_first = curr_pck->next;
		gf_free(curr_pck->data);
		gf_free(curr_pck);
	}
	if (st->curr_pck.data) gf_free(st->curr_pck.data);
	if (st->curr_pck.mpeg2_af_descriptors) gf_free(st->curr_pck.mpeg2_af_descriptors);
	if (st->mx) gf_mx_del(st->mx);
	if (st->loop_descriptors) {
		while (gf_list_count(st->loop_descriptors) ) {
			GF_M2TSDescriptor *desc = (GF_M2TSDescriptor*)gf_list_last(st->loop_descriptors);
			gf_list_rem_last(st->loop_descriptors);
			if (desc->data) gf_free(desc->data);
			gf_free(desc);
		}
		gf_list_del(st->loop_descriptors);
	}
	gf_free(st);
}

void gf_m2ts_mux_program_del(GF_M2TS_Mux_Program *prog)
{
	if (prog->iod) {
		gf_odf_desc_del(prog->iod);
	}
	while (prog->streams) {
		GF_M2TS_Mux_Stream *st = prog->streams->next;
		gf_m2ts_mux_stream_del(prog->streams);
		prog->streams = st;
	}
	if (prog->loop_descriptors) {
		while (gf_list_count(prog->loop_descriptors) ) {
			GF_M2TSDescriptor *desc = (GF_M2TSDescriptor*)gf_list_last(prog->loop_descriptors);
			gf_list_rem_last(prog->loop_descriptors);
			if (desc->data) gf_free(desc->data);
			gf_free(desc);
		}
		gf_list_del(prog->loop_descriptors);
	}
	gf_m2ts_mux_stream_del(prog->pmt);
	if (prog->name) gf_free(prog->name);
	if (prog->provider) gf_free(prog->provider);
	gf_free(prog);
}

GF_EXPORT
void gf_m2ts_program_stream_remove(GF_M2TS_Mux_Stream *stream)
{
	GF_M2TS_Mux_Program *program = stream->program;
	GF_M2TS_Mux_Stream *a_stream = program->streams;
	if (a_stream==stream) {
		program->streams = stream->next;
	} else {
		while (a_stream) {
			if (a_stream->next == stream) {
				a_stream->next = stream->next;
				break;
			}
			a_stream = a_stream->next;
		}
	}
	stream->next = NULL;
	gf_m2ts_mux_stream_del(stream);
	program->pmt->table_needs_update = GF_TRUE;
}

GF_EXPORT
void gf_m2ts_mux_del(GF_M2TS_Mux *mux)
{
	while (mux->programs) {
		GF_M2TS_Mux_Program *p = mux->programs->next;
		gf_m2ts_mux_program_del(mux->programs);
		mux->programs = p;
	}
	gf_m2ts_mux_stream_del(mux->pat);
	if (mux->sdt) gf_m2ts_mux_stream_del(mux->sdt);
	if (mux->pck_bs) gf_bs_del(mux->pck_bs);

	gf_free(mux);
}

GF_EXPORT
void gf_m2ts_mux_update_config(GF_M2TS_Mux *mux, Bool reset_time)
{
	GF_M2TS_Mux_Program *prog;

	gf_m2ts_mux_table_update_bitrate(mux, mux->pat);
	if (mux->sdt) {
		gf_m2ts_mux_table_update_bitrate(mux, mux->sdt);
	}

	if (!mux->fixed_rate) {
		mux->bit_rate = 0;

		/*get PAT bitrate*/
		mux->bit_rate += mux->pat->bit_rate;
		if (mux->sdt) {
			mux->bit_rate += mux->sdt->bit_rate;
		}
	}

	prog = mux->programs;
	while (prog) {
		GF_M2TS_Mux_Stream *stream = prog->streams;
		while (stream) {
			if (!mux->fixed_rate) {
				mux->bit_rate += stream->bit_rate;
			}
			/*reset mux time*/
			if (reset_time) stream->time.sec = stream->time.nanosec = 0;
			stream = stream->next;
		}
		/*get PMT bitrate*/
		gf_m2ts_mux_table_update_bitrate(mux, prog->pmt);

		if (!mux->fixed_rate) {
			mux->bit_rate += prog->pmt->bit_rate;
		}

		prog = prog->next;
	}

	/*reset mux time*/
	if (reset_time) {
		mux->time.sec = mux->time.nanosec = 0;
		mux->init_sys_time = 0;
	}

	if (!mux->bit_rate) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] No bitrates set in VBR mux mode, using 100kbps\n"));
		mux->bit_rate=100000;
	}
}

GF_EXPORT
u32 gf_m2ts_get_sys_clock(GF_M2TS_Mux *muxer)
{
	return (u32) (gf_sys_clock_high_res() - muxer->init_sys_time)/1000;
}

GF_EXPORT
u32 gf_m2ts_get_ts_clock(GF_M2TS_Mux *muxer)
{
	u32 now, init;
	init = muxer->init_ts_time.sec*1000 + muxer->init_ts_time.nanosec/1000000;
	now = muxer->time.sec*1000 + muxer->time.nanosec/1000000;
	return now-init;
}

GF_EXPORT
GF_Err gf_m2ts_mux_use_single_au_pes_mode(GF_M2TS_Mux *muxer, GF_M2TS_PackMode au_pes_mode)
{
	if (!muxer) return GF_BAD_PARAM;
	muxer->au_pes_mode = au_pes_mode;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m2ts_mux_set_initial_pcr(GF_M2TS_Mux *muxer, u64 init_pcr_value)
{
	if (!muxer) return GF_BAD_PARAM;
	muxer->init_pcr_value = 1 + init_pcr_value;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m2ts_mux_enable_pcr_only_packets(GF_M2TS_Mux *muxer, Bool enable_forced_pcr)
{
	if (!muxer) return GF_BAD_PARAM;
	muxer->enable_forced_pcr = enable_forced_pcr;
	return GF_OK;
}


GF_EXPORT
const u8 *gf_m2ts_mux_process(GF_M2TS_Mux *muxer, u32 *status, u32 *usec_till_next)
{
	GF_M2TS_Mux_Program *program;
	GF_M2TS_Mux_Stream *stream, *stream_to_process;
	GF_M2TS_Time time, max_time;
	u32 nb_streams, nb_streams_done;
	u64 now_us;
	char *ret;
	u32 res, highest_priority;
	Bool flush_all_pes = GF_FALSE;
	Bool check_max_time = GF_FALSE;

	nb_streams = nb_streams_done = 0;
	*status = GF_M2TS_STATE_IDLE;

	muxer->sap_inserted = GF_FALSE;
	muxer->last_pid = 0;

	now_us = gf_sys_clock_high_res();
	if (muxer->real_time) {
		if (!muxer->init_sys_time) {
			//init TS time
			muxer->time.sec = muxer->time.nanosec = 0;
			gf_m2ts_time_inc(&muxer->time, (u32) (muxer->init_pcr_value ? muxer->init_pcr_value-1 : 0), 27000000);
			muxer->init_sys_time = now_us;
			muxer->init_ts_time = muxer->time;
		} else {
			u64 us_diff = now_us - muxer->init_sys_time;
			GF_M2TS_Time now = muxer->init_ts_time;
			gf_m2ts_time_inc(&now, us_diff, 1000000);
			if (gf_m2ts_time_less(&now, &muxer->time)) {
				if (usec_till_next) {
					u32 diff = muxer->time.sec - now.sec;
					diff *= 1000000;
					if (now.nanosec <= muxer->time.nanosec) {
						diff += (muxer->time.nanosec - now.nanosec) / 1000;
					} else {
						assert(diff);
						diff -= 1000000;
						diff += (1000000000 + muxer->time.nanosec - now.nanosec) / 1000;
					}
					*usec_till_next = diff;
				}
				return NULL;
			}
		}
	}

	stream_to_process = NULL;
	time = muxer->time;
	max_time.sec = max_time.nanosec = 0;

	/*bitrate have changed*/
	if (muxer->needs_reconfig) {
		gf_m2ts_mux_update_config(muxer, GF_FALSE);
		muxer->needs_reconfig = GF_FALSE;
	}

	if (muxer->flush_pes_at_rap && muxer->force_pat) {
		program = muxer->programs;
		while (program) {
			stream = program->streams;
			while (stream) {
				if (stream->pes_data_remain) {
					flush_all_pes = GF_TRUE;
					break;
				}
				stream = stream->next;
			}
			program = program->next;
		}
	}

	if (!flush_all_pes) {

		/*compare PAT and PMT with current mux time
		if non-fixed rate, current mux time is the time of the last packet sent, time test is still valid - it will however not work
		if min access unit duration from all streams is greater than the PSI refresh rate*/

		/*PAT*/
		res = muxer->pat->process(muxer, muxer->pat);
		if ((res && gf_m2ts_time_less_or_equal(&muxer->pat->time, &time)) || muxer->force_pat) {
			time = muxer->pat->time;
			stream_to_process = muxer->pat;
			if (muxer->force_pat) {
				muxer->force_pat = GF_FALSE;
				muxer->force_pat_pmt_state = GF_SEG_BOUNDARY_FORCE_PMT;
			}
			/*force sending the PAT regardless of other streams*/
			goto send_pck;
		}

		/*SDT*/
		if (muxer->sdt && !muxer->force_pat_pmt_state) {
			res = muxer->sdt->process(muxer, muxer->sdt);
			if (res && gf_m2ts_time_less_or_equal(&muxer->sdt->time, &time) ) {
				time = muxer->sdt->time;
				stream_to_process = muxer->sdt;
				goto send_pck;
			}
		}

		/*PMT, for each program*/
		program = muxer->programs;
		while (program) {
			res = program->pmt->process(muxer, program->pmt);
			if ((res && gf_m2ts_time_less_or_equal(&program->pmt->time, &time)) || (muxer->force_pat_pmt_state==GF_SEG_BOUNDARY_FORCE_PMT)) {
				time = program->pmt->time;
				stream_to_process = program->pmt;
				if (muxer->force_pat_pmt_state==GF_SEG_BOUNDARY_FORCE_PMT)
					muxer->force_pat_pmt_state = GF_SEG_BOUNDARY_FORCE_PCR;
				/*force sending the PMT regardless of other streams*/
				goto send_pck;
			}
			program = program->next;
		}
	}

	/*if non-fixed rate, just pick the earliest data on all streams*/
	if (!muxer->fixed_rate) {
		if (!muxer->real_time) {
			time.sec = 0xFFFFFFFF;
		} else {
			check_max_time = GF_TRUE;
		}
	}

#define FORCE_PCR_FIRST	0

#if FORCE_PCR_FIRST

	/*PCR stream, for each program (send them first to avoid PCR to never be sent)*/
	program = muxer->programs;
	while (program) {
		stream = program->pcr;
		if (!flush_all_pes  || (stream->copy_from_next_packets + stream->pes_data_remain) ) {
			res = stream->process(muxer, stream);
			if (res && gf_m2ts_time_less_or_equal(&stream->time, &time)) {
				time = stream->time;
				stream_to_process = stream;
				goto send_pck;
			}
		}
		program = program->next;
	}
#endif

	/*all streams for each program*/
	highest_priority = 0;
	program = muxer->programs;
	while (program) {
		stream = program->streams;
		while (stream) {
#if FORCE_PCR_FIRST
			if (stream != program->pcr)
#endif
			{
				if (flush_all_pes && !stream->pes_data_remain) {
					nb_streams++;
					stream = stream->next;
					continue;
				}

				res = stream->process(muxer, stream);
				/*next is rap on this stream, check flushing of other pes (we could use a goto)*/
				if (!flush_all_pes && muxer->force_pat)
					return gf_m2ts_mux_process(muxer, status, usec_till_next);

				if (res) {
					/*always schedule the earliest data*/
					if (gf_m2ts_time_less(&stream->time, &time)) {
						highest_priority = res;
						time = stream->time;
						stream_to_process = stream;
#if FORCE_PCR_FIRST
						goto send_pck;
#endif
					}
					else if (gf_m2ts_time_equal(&stream->time, &time)) {
						/*if the same priority schedule base stream first*/
						if ((res > highest_priority) || ((res == highest_priority) && !stream->ifce->depends_on_stream)) {
							highest_priority = res;
							time = stream->time;
							stream_to_process = stream;
#if FORCE_PCR_FIRST
							goto send_pck;
#endif
						}
					} else if (check_max_time && gf_m2ts_time_less(&max_time, &stream->time)) {
						max_time = stream->time;
					}
				}
			}
			nb_streams++;
			if ((stream->ifce->caps & GF_ESI_STREAM_IS_OVER) && (!res || stream->refresh_rate_ms) )
				nb_streams_done ++;

			stream = stream->next;
		}
		program = program->next;
	}

send_pck:

	ret = NULL;
	if (!stream_to_process) {
		if (nb_streams && (nb_streams==nb_streams_done)) {
			*status = GF_M2TS_STATE_EOS;
		} else {
			*status = GF_M2TS_STATE_PADDING;
		}
		/* padding packets ?? */
		if (muxer->fixed_rate) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Inserting empty packet at %d:%09d\n", time.sec, time.nanosec));
			ret = muxer->null_pck;
			muxer->tot_pad_sent++;
		}
	} else {

		if (stream_to_process->tables) {
			gf_m2ts_mux_table_get_next_packet(muxer, stream_to_process, muxer->dst_pck);
		} else {
			gf_m2ts_mux_pes_get_next_packet(stream_to_process, muxer->dst_pck);
			if (stream_to_process->pck_sap_type) {
				muxer->sap_inserted = GF_TRUE;
				muxer->sap_type = stream_to_process->pck_sap_type;
				muxer->sap_time = stream_to_process->pck_sap_time;
			}
			muxer->last_pts = stream_to_process->curr_pck.cts;
			muxer->last_pid = stream_to_process->pid;
		}

		ret = muxer->dst_pck;
		*status = GF_M2TS_STATE_DATA;

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sending %s from PID %d at %d:%09d - mux time %d:%09d\n", stream_to_process->tables ? "table" : "PES", stream_to_process->pid, time.sec, time.nanosec, muxer->time.sec, muxer->time.nanosec));

		if (nb_streams && (nb_streams==nb_streams_done))
			*status = GF_M2TS_STATE_EOS;
	}
	if (ret) {
		muxer->tot_pck_sent++;
		/*increment time*/
		if (muxer->fixed_rate ) {
			gf_m2ts_time_inc(&muxer->time, 1504/*188*8*/, muxer->bit_rate);
		}
		else if (muxer->real_time) {
			u64 us_diff = gf_sys_clock_high_res() - muxer->init_sys_time;
			muxer->time = muxer->init_ts_time;
			gf_m2ts_time_inc(&muxer->time, us_diff, 1000000);
		}
		/*if a stream was found, use it*/
		else if (stream_to_process) {
			muxer->time = time;
		}

		muxer->pck_sent_over_br_window++;
		if (now_us - muxer->last_br_time_us > 1000000) {
			u64 size = 8*188*muxer->pck_sent_over_br_window;
			muxer->average_birate_kbps = (u32) (size*1000 / (now_us - muxer->last_br_time_us));
			muxer->last_br_time_us = now_us;
			muxer->pck_sent_over_br_window=0;
		}
	} else if (muxer->real_time && !muxer->fixed_rate) {
//		u64 us_diff = gf_sys_clock_high_res() - muxer->init_sys_time;
//		muxer->time = muxer->init_ts_time;
//		gf_m2ts_time_inc(&muxer->time, us_diff, 1000000);
		muxer->time = max_time;
	}
	return ret;
}

#endif /*GPAC_DISABLE_MPEG2TS_MUX*/

