/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre , Cyril Concolato, Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2000-2012
 *					All rights reserved
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

#include <gpac/mpegts.h>
#include <gpac/constants.h>
#include <gpac/media_tools.h>

#if !defined(GPAC_DISABLE_MPEG2TS_MUX)

/*num ms between PCR*/
#define PCR_UPDATE_MS	200
/*90khz internal delay between two updates for instant bitrate compute per stream*/
#define BITRATE_UPDATE_WINDOW	90000
/* length of adaptation_field_length; */ 
#define ADAPTATION_LENGTH_LENGTH 1
/* discontinuty flag, random access flag ... */
#define ADAPTATION_FLAGS_LENGTH 1 
/* length of encoded pcr */
#define PCR_LENGTH 6


static GFINLINE Bool gf_m2ts_time_less(GF_M2TS_Time *a, GF_M2TS_Time *b) {
	if (a->sec>b->sec) return 0;
	if (a->sec==b->sec) return (a->nanosec<b->nanosec) ? 1 : 0;
	return 1;
}
static GFINLINE Bool gf_m2ts_time_less_or_equal(GF_M2TS_Time *a, GF_M2TS_Time *b) {
	if (a->sec>b->sec) return 0;
	if (a->sec==b->sec) return (a->nanosec>b->nanosec) ? 0 : 1;
	return 1;
}

static GFINLINE void gf_m2ts_time_inc(GF_M2TS_Time *time, u32 delta_inc_num, u32 delta_inc_den)
{
	u64 n_sec;
	u32 sec;

	/*couldn't compute bitrate - we need to have more info*/
	if (!delta_inc_den) return;

	sec = delta_inc_num / delta_inc_den;

	if (sec) {
		time->sec += sec;
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
		table->table_id = table_id;
		if (prev_table) prev_table->next = table;
		else stream->tables = table;
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

		gf_bs_get_content(bs, (char**) &section->data, &section->length); 
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
		char *slhdr;
		u32 slhdr_size;		
		GF_SAFEALLOC(section, GF_M2TS_Mux_Section);

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

		gf_bs_get_content(bs, (char**) &section->data, &section->length); 
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
	stream->table_needs_send = 1;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Generating %d sections for MPEG-4 SL packet - version number %d - extension ID %d\n", stream->pid, nb_sections, table->version_number, table_id_extension));

	/*MPEG-4 tables are input streams for the mux, the bitrate is updated when fetching AUs*/
}

static u32 gf_m2ts_add_adaptation(GF_M2TS_Mux_Program *prog, GF_BitStream *bs, u16 pid,  
				   Bool has_pcr, u64 pcr_time, 
				   Bool is_rap, 
				   u32 padding_length)
{
	u32 adaptation_length;

	adaptation_length = ADAPTATION_FLAGS_LENGTH + (has_pcr?PCR_LENGTH:0) + padding_length;

	gf_bs_write_int(bs,	adaptation_length, 8);
	gf_bs_write_int(bs,	0, 1);			// discontinuity indicator
	gf_bs_write_int(bs,	is_rap, 1);		// random access indicator
	gf_bs_write_int(bs,	0, 1);			// es priority indicator
	gf_bs_write_int(bs,	has_pcr, 1);	// PCR_flag 
	gf_bs_write_int(bs,	0, 1);			// OPCR flag
	gf_bs_write_int(bs,	0, 1);			// splicing point flag
	gf_bs_write_int(bs,	0, 1);			// transport private data flag
	gf_bs_write_int(bs,	0, 1);			// adaptation field extension flag
	if (has_pcr) {
		u64 PCR_base, PCR_ext;
		PCR_base = pcr_time/300;
		gf_bs_write_long_int(bs, PCR_base, 33); 
		gf_bs_write_int(bs,	0, 6); // reserved
		PCR_ext = pcr_time - PCR_base*300;
		gf_bs_write_long_int(bs, PCR_ext, 9);
		if (prog->last_pcr > pcr_time) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Sending PCR "LLD" earlier than previous PCR "LLD" - drift %f sec\n", pid, pcr_time, prog->last_pcr, (prog->last_pcr - pcr_time) /27000000.0 ));
		}
		prog->last_pcr = pcr_time;

#ifndef GPAC_DISABLE_LOG
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Adding adaptation field size %d - RAP %d - Padding %d - PCR "LLD"\n", pid, adaptation_length, is_rap, padding_length, pcr_time));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Adding adaptation field size %d - RAP %d - Padding %d\n", pid, adaptation_length, is_rap, padding_length));
#endif

	}

	gf_bs_write_byte(bs, 0xFF, padding_length); // stuffing byte
	
	return adaptation_length + ADAPTATION_LENGTH_LENGTH;
}

void gf_m2ts_mux_table_get_next_packet(GF_M2TS_Mux_Stream *stream, char *packet)
{
	GF_BitStream *bs;
	GF_M2TS_Mux_Table *table;
	GF_M2TS_Mux_Section *section;
	u32 payload_length, padding_length;
	u8 adaptation_field_control;

	stream->table_needs_send = 0;
	table = stream->current_table;
	assert(table);

	section = stream->current_section;
	assert(section);

	bs = gf_bs_new(packet, 188, GF_BITSTREAM_WRITE);
	
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

	if (section->length - stream->current_section_offset >= payload_length) {
		padding_length = 0;
		adaptation_field_control = GF_M2TS_ADAPTATION_NONE;
	} else {		
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
	}
	assert(payload_length + stream->current_section_offset <= section->length);
	
	gf_bs_write_int(bs,	0, 1);    /*priority indicator*/
	gf_bs_write_int(bs,	stream->pid, 13); /*pid*/
	gf_bs_write_int(bs,	0, 2);    /*scrambling indicator*/
	gf_bs_write_int(bs,	adaptation_field_control, 2);    /*we do not use adaptation field for sections */
	gf_bs_write_int(bs,	stream->continuity_counter, 4);   /*continuity counter*/
	if (stream->continuity_counter < 15) stream->continuity_counter++;
	else stream->continuity_counter=0;

	if (adaptation_field_control != GF_M2TS_ADAPTATION_NONE)
		gf_m2ts_add_adaptation(stream->program, bs, stream->pid, 0, 0, 0, padding_length);

	/*pointer field*/
	if (!stream->current_section_offset) {
		/* no concatenations of sections in ts packets, so start address is 0 */
		gf_bs_write_u8(bs, 0);
	}
	gf_bs_del(bs);

	memcpy(packet+188-payload_length, section->data + stream->current_section_offset, payload_length);
	stream->current_section_offset += payload_length;
	
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
		gf_bs_get_content(bs, (char**)&payload, &size);
		gf_bs_del(bs);
		gf_m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PAT, muxer->ts_id, payload, size, 1, 0, 0);
		stream->table_needs_update = 0;
		stream->table_needs_send = 1;
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
				GF_M2TSDescriptor *desc = gf_list_get(stream->program->loop_descriptors, i);
				info_length += 2 + desc->data_len;
			}
		}

		if (!stream->program->iod) {
			gf_bs_write_int(bs,	info_length, 12); // program info length =0
		} else {
			u32 len, i;
			GF_ESD *esd;
			GF_BitStream *bs_iod;
			char *iod_data;
			u32 iod_data_len;

			/*rewrite SL config in IOD streams*/
			i=0;
			while (NULL != (esd = gf_list_enum(((GF_ObjectDescriptor*)stream->program->iod)->ESDescriptors, &i))) {
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
				GF_M2TSDescriptor *desc = gf_list_get(stream->program->loop_descriptors, i);
				gf_bs_write_int(bs,	desc->tag, 8);  
				gf_bs_write_int(bs,	desc->data_len, 8);  
				gf_bs_write_data(bs, desc->data, desc->data_len);  
			}
		}

		es = stream->program->streams;
		while (es) {
			Bool has_lang = 0;
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
					GF_M2TSDescriptor *desc = gf_list_get(es->loop_descriptors, i);
					es_info_length += 2 +desc->data_len;
				}
			}

			if (es->ifce && es->ifce->lang && (es->ifce->lang  != GF_4CC('u', 'n', 'd', ' ')) ) {
				es_info_length += 2 + 3;
				has_lang = 1;
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
				gf_bs_write_int(bs,	0x41, 8); 
				gf_bs_write_int(bs,	0x43, 8); 
				gf_bs_write_int(bs,	0x2D, 8); 
				gf_bs_write_int(bs,	0x33, 8); 
				break;
			case GF_M2TS_VIDEO_VC1:
				gf_bs_write_int(bs,	GF_M2TS_REGISTRATION_DESCRIPTOR, 8); 
				gf_bs_write_int(bs,	4, 8); 
				gf_bs_write_int(bs,	0x56, 8); 
				gf_bs_write_int(bs,	0x43, 8); 
				gf_bs_write_int(bs,	0x2D, 8); 
				gf_bs_write_int(bs,	0x31, 8); 
				break;
			case GF_M2TS_AUDIO_EC3:
				gf_bs_write_int(bs,	GF_M2TS_DVB_EAC3_DESCRIPTOR, 8); 
				gf_bs_write_int(bs,	0, 8); //check what is in this desc
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
	
		gf_bs_get_content(bs, (char**)&payload, &length);
		gf_bs_del(bs);

		gf_m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PMT, stream->program->number, payload, length, 1, 0, 0);
		stream->table_needs_update = 0;
		stream->table_needs_send = 1;
		gf_free(payload);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Updating PMT - Program Number %d - %d streams - size %d%s\n", stream->pid, stream->program->number, nb_streams, length, stream->program->iod ? " - MPEG-4 Systems detected":""));
	}
	if (stream->table_needs_send) 
		return 1;
	if (stream->refresh_rate_ms) 
		return 1;
	return 0;
}

static void gf_m2ts_remap_timestamps_for_pes(GF_M2TS_Mux_Stream *stream, u32 pck_flags, u64 *dts, u64 *cts)
{
	u64 pcr_offset;

	/*Rescale our timestamps and express them in PCR*/
	if (stream->ts_scale) {
		*cts = (u64) (stream->ts_scale * (s64) *cts);
		*dts = (u64) (stream->ts_scale * (s64) *dts);
	}
	if (!stream->program->initial_ts_set) {
		u32 nb_bits = (u32) (stream->program->mux->tot_pck_sent - stream->program->num_pck_at_pcr_init) * 1504;
		u32 nb_ticks = 90000*nb_bits / stream->program->mux->bit_rate;
		stream->program->initial_ts = *dts;

		if (stream->program->initial_ts > nb_ticks)
			stream->program->initial_ts -= nb_ticks;
		else
			stream->program->initial_ts = 0;

		stream->program->initial_ts_set = 1;
	}
	else if (*dts < stream->program->initial_ts) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: DTS "LLD" is less than initial DTS "LLD" - adjusting\n", stream->pid, *dts, stream->program->initial_ts));
		stream->program->initial_ts = *dts;
	}
	else if (*dts < stream->last_dts) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: DTS "LLD" is less than last sent DTS "LLD"\n", stream->pid, *dts, stream->last_dts));
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

u32 gf_m2ts_stream_process_stream(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
{
	Bool ret = 0;

	if (stream->mpeg2_stream_type==GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
		/*section has just been updated */
		if (stream->table_needs_send)
			return stream->scheduling_priority;
		/*section is not completely sent yet or this is not the first section of the table*/
		if (stream->current_section && (stream->current_section_offset || stream->current_section!=stream->current_table->section)) 
			return stream->scheduling_priority;
		if (stream->ifce->repeat_rate && stream->tables)
			ret = stream->program->pcr_init_time ? stream->scheduling_priority : 0;
	}
	else if (stream->curr_pck.data_len && stream->pck_offset < stream->curr_pck.data_len) {
		/*PES packet not completely sent yet*/
		return stream->scheduling_priority + stream->pcr_priority;
	}

	stream->pcr_priority = 0;
	/*PULL mode*/
	if (stream->ifce->caps & GF_ESI_AU_PULL_CAP) {
		if (stream->curr_pck.data_len) {
			/*discard packet data if we use SL over PES*/
			if (stream->discard_data) gf_free(stream->curr_pck.data);
			/*release data*/
			stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_RELEASE, NULL);
		}
		stream->pck_offset = 0;
		stream->curr_pck.data_len = 0;
		stream->discard_data = 0;

		/*EOS*/
		if (stream->ifce->caps & GF_ESI_STREAM_IS_OVER) return ret;
		assert( stream->ifce->input_ctrl);
		stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_PULL, &stream->curr_pck);
	} else {
		GF_M2TS_Packet *curr_pck;

		if (!stream->pck_first && (stream->ifce->caps & GF_ESI_STREAM_IS_OVER)) 
			return ret;
		
		/*flush input pipe*/
		if (stream->ifce->input_ctrl) stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_FLUSH, NULL);

		gf_mx_p(stream->mx);

		stream->pck_offset = 0;
		stream->curr_pck.data_len = 0;

		/*fill curr_pck*/
		curr_pck = stream->pck_first;
		if (!curr_pck) {
			gf_mx_v(stream->mx);
			return ret;
		}
		stream->curr_pck.cts = curr_pck->cts;
		stream->curr_pck.data = curr_pck->data;
		stream->curr_pck.data_len = curr_pck->data_len;
		stream->curr_pck.dts = curr_pck->dts;
		stream->curr_pck.flags = curr_pck->flags;

		/*discard first packet*/
		stream->pck_first = curr_pck->next;
		gf_free(curr_pck);
		stream->discard_data = 1;

		gf_mx_v(stream->mx);
	}

	if (!(stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS))
		stream->curr_pck.dts = stream->curr_pck.cts;

	/*initializing the PCR*/
	if (!stream->program->pcr_init_time) {
		if (stream==stream->program->pcr) {
			if (stream->program->mux->init_pcr_value) {
				stream->program->pcr_init_time = stream->program->mux->init_pcr_value;
			} else {
				while (!stream->program->pcr_init_time)
					stream->program->pcr_init_time = gf_rand();
			}

			stream->program->ts_time_at_pcr_init = muxer->time;
			stream->program->num_pck_at_pcr_init = muxer->tot_pck_sent;

			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Initializing PCR for program number %d: PCR %d - mux time %d:%09d\n", stream->pid, stream->program->number, stream->program->pcr_init_time, muxer->time.sec, muxer->time.nanosec));
		} else {
			/*PES has been sent, discard internal buffer*/
			if (stream->discard_data) gf_free(stream->curr_pck.data);
			stream->curr_pck.data = NULL;
			stream->curr_pck.data_len = 0;
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
		stream->sl_header.randomAccessPointFlag = (stream->curr_pck.flags & GF_ESI_DATA_AU_RAP) ? 1: 0;
		stream->sl_header.compositionTimeStampFlag = (stream->curr_pck.flags & GF_ESI_DATA_HAS_CTS) ? 1 : 0;
		stream->sl_header.compositionTimeStamp = stream->curr_pck.cts;
		stream->sl_header.decodingTimeStampFlag = (stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS) ? 1: 0;
		stream->sl_header.decodingTimeStamp = stream->curr_pck.dts;

		gf_m2ts_mux_table_update_mpeg4(stream, stream->table_id, muxer->ts_id, stream->curr_pck.data, stream->curr_pck.data_len, 1, 0, (stream->curr_pck.flags & GF_ESI_DATA_REPEAT) ? 0 : 1, 0);

		/*packet data is now copied in sections, discard it if not pull*/
		if (!(stream->ifce->caps & GF_ESI_AU_PULL_CAP)) {
			gf_free(stream->curr_pck.data);
			stream->curr_pck.data = NULL;
			stream->curr_pck.data_len = 0;
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
		stream->sl_header.randomAccessPointFlag = (stream->curr_pck.flags & GF_ESI_DATA_AU_RAP) ? 1: 0;
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
		stream->discard_data = 1;
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
		stream->discard_data = 1;
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
		stream->discard_data = 1;
		break;
	}

	if (stream->start_pes_at_rap && (stream->curr_pck.flags & GF_ESI_DATA_AU_RAP)
		) {
		stream->program->mux->force_pat_pmt_state = GF_SEG_BOUNDARY_FORCE_PAT;
		stream->program->mux->force_pat = GF_TRUE;
	}

	/*rewrite timestamps for PES header*/
	gf_m2ts_remap_timestamps_for_pes(stream, stream->curr_pck.flags, &stream->curr_pck.dts, &stream->curr_pck.cts);


	/*compute next interesting time in TS unit: this will be DTS of next packet*/
	stream->time = stream->program->ts_time_at_pcr_init;
	gf_m2ts_time_inc(&stream->time, (u32) stream->curr_pck.dts, 90000);

	/*do we need to send a PCR*/
	if (stream == stream->program->pcr) {
		if (muxer->real_time) {
			if (gf_sys_clock() > stream->program->last_sys_clock + PCR_UPDATE_MS)
				stream->pcr_priority = 1;
		} else {
			if (!stream->program->last_dts || (stream->curr_pck.dts > stream->program->last_dts + PCR_UPDATE_MS*90))
				stream->pcr_priority = 1;
		}
	}
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Next data schedule for %d:%09d - mux time %d:%09d\n", stream->pid, stream->time.sec, stream->time.nanosec, muxer->time.sec, muxer->time.nanosec));

	/*compute instant bitrate*/
	if (!stream->last_br_time) {
		stream->last_br_time = stream->curr_pck.dts + 1;
		stream->bytes_since_last_time = 0;
		stream->pes_since_last_time = 0;
	} else {
		u32 time_diff = (u32) (stream->curr_pck.dts - stream->last_br_time - 1 );
		if ((stream->pes_since_last_time > 4) && (time_diff >= BITRATE_UPDATE_WINDOW)) {
			u32 bitrate;
			u64 r = 8*stream->bytes_since_last_time;
			r*=90000;
			bitrate = (u32) (r / time_diff);
			stream->bit_rate = bitrate;
			stream->last_br_time = 0;
			stream->bytes_since_last_time = 0;
			stream->pes_since_last_time = 0;
			stream->program->mux->needs_reconfig = 1;
		}
	} 
	stream->pes_since_last_time ++;
	return stream->scheduling_priority + stream->pcr_priority;
}

static GFINLINE u64 gf_m2ts_get_pcr(GF_M2TS_Mux_Program *program)
{
	u32 nb_pck = (u32) (program->mux->tot_pck_sent - program->num_pck_at_pcr_init);
	u64 pcr = 27000000;
	pcr *= nb_pck*1504;
	pcr /= program->mux->bit_rate;
	pcr += program->pcr_init_time;
	return pcr;
}

void gf_m2ts_stream_update_data_following(GF_M2TS_Mux_Stream *stream)
{
	Bool ignore_next=0;
	stream->next_payload_size = 0;
	stream->next_pck_flags = 0;
	stream->copy_from_next_packets = 0;
	
	if (stream->program->mux->one_au_per_pes) return;

	switch (stream->mpeg2_stream_type) {
	/*the following stream types do not allow PES boundaries at any place on their payload*/
	case GF_M2TS_SYSTEMS_MPEG4_PES:
		/*one and only one SL per PES: we cannot concatenate*/
		return;
	default:
		break;
	}

	if (stream->ifce->caps & GF_ESI_AU_PULL_CAP) {
		GF_ESIPacket test_pck;
		test_pck.data_len = 0;
		/*pull next data but do not release it since it might be needed later on*/
		stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_PULL, &test_pck);
		if (test_pck.data_len) {
			stream->next_payload_size = test_pck.data_len;
			stream->next_pck_flags = test_pck.flags;
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
		}
	}
	/*consider we don't have the next AU if:
	1- we are asked to start new PES at RAP, just consider we don't have the next AU*/
	if (stream->start_pes_at_rap && (stream->next_pck_flags & GF_ESI_DATA_AU_RAP) ) {
		ignore_next=1;
//		stream->program->mux->force_pat_pmt_state = GF_SEG_BOUNDARY_START;
	}
	/*if we have a RAP about to start on a stream in this program, force all other streams to stop merging data in their current PES*/
	else if (stream->program->mux->force_pat_pmt_state) {
		ignore_next=1;
	}

	if (ignore_next) {
		stream->next_payload_size = 0;
		stream->next_pck_cts = 0;
		stream->next_pck_dts = 0;
		stream->next_pck_flags = 0;
	} 

	if (stream->next_payload_size) {
		stream->next_payload_size += stream->reframe_overhead;

		gf_m2ts_remap_timestamps_for_pes(stream, stream->next_pck_flags, &stream->next_pck_dts, &stream->next_pck_cts);
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
			/*don't end next AU in next PES if we don't want to start 2 AUs in one PES*/
			if (stream->prevent_two_au_start_in_pes && (ts_bytes>pck_size + stream->next_payload_size)) {
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
			/*if we don't have enough space in the PES to store begining of new AU, don't copy it and ask
			to recompute header (we might no longer have DTS/CTS signaled)*/
			if (stream->copy_from_next_packets < stream->min_bytes_copy_from_next) {
				stream->copy_from_next_packets = 0;
				stream->next_payload_size = 0;
				stream->next_pck_flags = 0;
				return 0;
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
			return 0;
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
	return 1;
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
	} else {
		hdr_len = hdr_len;
	}
	return hdr_len;
}

u32 gf_m2ts_stream_add_pes_header(GF_BitStream *bs, GF_M2TS_Mux_Stream *stream, u32 payload_length)
{
	u64 t, dts, cts;
	u32 pes_len;
	Bool use_pts, use_dts;
	
	gf_bs_write_int(bs,	0x1, 24);//packet start code
	gf_bs_write_u8(bs,	stream->mpeg2_stream_id);// stream id 

	/*next AU start in current PES and current AU began in previous PES, use next AU timing*/
	if (stream->pck_offset && stream->copy_from_next_packets) {
		use_pts = (stream->next_pck_flags & GF_ESI_DATA_HAS_CTS) ? 1 : 0;
		use_dts = (stream->next_pck_flags & GF_ESI_DATA_HAS_DTS) ? 1 : 0;
		dts = stream->next_pck_dts;
		cts = stream->next_pck_cts;
	}
	/*we already sent the begining of the AU*/
	else if (stream->pck_offset) {
		use_pts = use_dts = 0;
		dts = cts = 0;
	} else {
		use_pts = (stream->curr_pck.flags & GF_ESI_DATA_HAS_CTS) ? 1 : 0;
		use_dts = (stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS) ? 1 : 0;
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
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Adding PES header at PCR "LLD" - has PTS %d (%d) - has DTS %d (%d)\n", stream->pid, gf_m2ts_get_pcr(stream->program)/300, use_pts, cts, use_dts, dts));

	return pes_len+4; // 4 = start code + stream_id
}

void gf_m2ts_mux_pes_get_next_packet(GF_M2TS_Mux_Stream *stream, char *packet)
{
	GF_BitStream *bs;
	Bool needs_pcr, first_pass, is_rap=0;
	u32 adaptation_field_control, payload_length, payload_to_copy, padding_length, hdr_len, pos, copy_next;

	assert(stream->pid);
	bs = gf_bs_new(packet, 188, GF_BITSTREAM_WRITE);

	hdr_len = gf_m2ts_stream_get_pes_header_length(stream);

	/*we may need two pass in case we first compute hdr len and TS payload size by considering
	we concatenate next au start in this PES but finally couldn't do it when computing PES len
	and AU alignment constraint of the stream*/
	first_pass = 1;
	while (1) {
		if (hdr_len) {
			if (first_pass)
				gf_m2ts_stream_update_data_following(stream);
			hdr_len = gf_m2ts_stream_get_pes_header_length(stream);
		}
		
		adaptation_field_control = GF_M2TS_ADAPTATION_NONE;
		payload_length = 184 - hdr_len;
		payload_to_copy = padding_length = 0;
		is_rap = (hdr_len && (stream->curr_pck.flags & GF_ESI_DATA_AU_RAP)) ? GF_TRUE : GF_FALSE;
		needs_pcr = (hdr_len && (stream == stream->program->pcr) && (stream->pcr_priority || is_rap) ) ? 1 : 0;

		/*if we forced inserting PAT/PMT before new RAP, also insert PCR here*/
		if (stream->program->mux->force_pat_pmt_state == GF_SEG_BOUNDARY_FORCE_PCR) {
			if (stream == stream->program->pcr) {
				stream->program->mux->force_pat_pmt_state = GF_SEG_BOUNDARY_NONE;
				needs_pcr = 1;
			}
		}

 		if (needs_pcr) {
			/*AF headers + PCR*/
			payload_length -= 8;
			adaptation_field_control = GF_M2TS_ADAPTATION_AND_PAYLOAD;
		}
		
		if (hdr_len) {
			assert(!stream->pes_data_remain);
			if (! gf_m2ts_stream_compute_pes_length(stream, payload_length)) {
				first_pass = 0;
				continue;
			}

			assert(stream->pes_data_remain==stream->pes_data_len);
		}
		break;
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
			payload_length -= 2;
			adaptation_field_control = GF_M2TS_ADAPTATION_AND_PAYLOAD;
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
		Bool is_rap;
		u64 pcr = 0;
		if (needs_pcr) {
			u32 now = gf_sys_clock();
			/*compute PCR - we disabled real-time clock for now and only insert PCR based on DTS / CTS*/
			if (0 && stream->program->mux->real_time) {
				pcr = gf_m2ts_get_pcr(stream->program);
			} else {
				pcr = ( ((stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS) ? stream->curr_pck.dts : stream->curr_pck.cts) - stream->program->pcr_offset) * 300;
				if (pcr>stream->program->pcr_init_time) pcr -= stream->program->pcr_init_time;
				else pcr = 0;
			}

			//fprintf(stderr, "PCR Diff in ms %d - sys clock diff in ms %d - DTS diff %d\n", (u32) (pcr - stream->program->last_pcr) / 27000, now - stream->program->last_sys_clock, (stream->curr_pck.dts - stream->program->last_dts)/90);

			stream->program->last_sys_clock = now;
			/*if stream does not use DTS, use CTS as base time for PCR*/
			stream->program->last_dts = (stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS) ? stream->curr_pck.dts : stream->curr_pck.cts;
			stream->pcr_priority = 0;
		}
		is_rap = (hdr_len && (stream->curr_pck.flags & GF_ESI_DATA_AU_RAP) ) ? 1 : 0;
		gf_m2ts_add_adaptation(stream->program, bs, stream->pid, needs_pcr, pcr, is_rap, padding_length);
	
		if (padding_length) 
			stream->program->mux->tot_pes_pad_bytes += padding_length;

	}

	if (hdr_len) gf_m2ts_stream_add_pes_header(bs, stream, payload_length);

	pos = (u32) gf_bs_get_position(bs);
	gf_bs_del(bs);

	assert(stream->curr_pck.data_len - stream->pck_offset >= payload_to_copy);
	memcpy(packet+pos, stream->curr_pck.data + stream->pck_offset, payload_to_copy);
	stream->pck_offset += payload_to_copy;
	assert(stream->pes_data_remain >= payload_to_copy);
	stream->pes_data_remain -= payload_to_copy;

	/*update stream time, including headers*/
//	gf_m2ts_time_inc(&stream->time, payload_to_copy + pos - 4, stream->bit_rate);
	
	if (stream->pck_offset == stream->curr_pck.data_len) {
		/*PES has been sent, discard internal buffer*/
		if (stream->discard_data) gf_free(stream->curr_pck.data);
		stream->curr_pck.data = NULL;
		stream->curr_pck.data_len = 0;
		stream->pck_offset = 0;

		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Done sending PES (%d bytes) from PID %d at stream time %d:%d (DTS "LLD" - PCR "LLD")\n", stream->curr_pck.data_len, stream->pid, stream->time.sec, stream->time.nanosec, stream->curr_pck.dts, gf_m2ts_get_pcr(stream->program)/300));

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_CONTAINER, GF_LOG_INFO)
			&& gf_m2ts_time_less(&stream->program->mux->time, &stream->time)
		) {
			s32 drift;
			GF_M2TS_Time muxtime = stream->program->mux->time;
			drift= stream->time.nanosec;
			drift-=muxtime.nanosec;
			drift/=1000;
			if (muxtime.sec!=stream->time.sec) {
				drift += (stream->time.sec - muxtime.sec)*1000000;
			}
			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] PES PID %d sent %d us too late\n", stream->pid, drift) );
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
					stream->pck_offset = 0;
				}
				if (!remain) break;
				pos += copy_next;
				copy_next = remain;
			}
		}
		else if (stream->program->mux->force_pat_pmt_state==GF_SEG_BOUNDARY_START) {
			stream->program->mux->force_pat_pmt_state = GF_SEG_BOUNDARY_FORCE_PAT;
			stream->program->mux->force_pat = 1;
		}
	}
	stream->bytes_since_last_time += 188;
}


GF_M2TS_Mux_Stream *gf_m2ts_stream_new(u32 pid) {
	GF_M2TS_Mux_Stream *stream;

	GF_SAFEALLOC(stream, GF_M2TS_Mux_Stream);
	stream->pid = pid;
	stream->process = gf_m2ts_stream_process_stream;

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
				gf_mx_p(stream->mx);
				if (!stream->pck_first) {
					stream->pck_first = stream->pck_last = stream->pck_reassembler;
				} else {
					stream->pck_last->next = stream->pck_reassembler;
					stream->pck_last = stream->pck_reassembler;
				}
				gf_mx_v(stream->mx);
				stream->pck_reassembler = NULL;
			}
		}
		if (!stream->pck_reassembler) {
			GF_SAFEALLOC(stream->pck_reassembler, GF_M2TS_Packet);
			stream->pck_reassembler->cts = esi_pck->cts;
			stream->pck_reassembler->dts = esi_pck->dts;
		}

		stream->force_new = esi_pck->flags & GF_ESI_DATA_AU_END ? 1 : 0;

		stream->pck_reassembler->data = gf_realloc(stream->pck_reassembler->data , sizeof(char)*(stream->pck_reassembler->data_len+esi_pck->data_len) );
		memcpy(stream->pck_reassembler->data + stream->pck_reassembler->data_len, esi_pck->data, esi_pck->data_len);
		stream->pck_reassembler->data_len += esi_pck->data_len;

		stream->pck_reassembler->flags |= esi_pck->flags;
		if (stream->force_new) {
			gf_mx_p(stream->mx);
			if (!stream->pck_first) {
				stream->pck_first = stream->pck_last = stream->pck_reassembler;
			} else {
				stream->pck_last->next = stream->pck_reassembler;
				stream->pck_last = stream->pck_reassembler;
			}
			gf_mx_v(stream->mx);
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

static u32 gf_m2ts_stream_get_pid(GF_M2TS_Mux_Program *program, u32 stream_id)
{
	GF_M2TS_Mux_Stream *st;

	st = program->streams;

	while (st)
	{
		if (st->ifce->stream_id == stream_id)
			return st->pid;
		st = st->next;
	}

	return 0;
}


static void gf_m2ts_stream_add_hierarchy_descriptor(GF_M2TS_Mux_Stream *stream)
{
	GF_M2TSDescriptor *desc;
	GF_BitStream *bs;
	u32 data_len;

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
	gf_bs_write_int(bs, stream->pid - stream->program->pmt->pid, 6);
	/*tref_present_flag = 1 : NOT PRESENT*/
	gf_bs_write_int(bs, 1, 1);
	/*reserved*/
	gf_bs_write_int(bs, 1, 1);
	/*hierarchy_embedded_layer_index*/
	gf_bs_write_int(bs, gf_m2ts_stream_get_pid(stream->program, stream->ifce->depends_on_stream) - stream->program->pmt->pid, 6);
	/*reserved*/
	gf_bs_write_int(bs, 3, 2);
	/*hierarchy_channel*/
	gf_bs_write_int(bs, stream->ifce->stream_id, 6);

	GF_SAFEALLOC(desc, GF_M2TSDescriptor);
	desc->tag = (u8) GF_M2TS_HIERARCHY_DESCRIPTOR;
	gf_bs_get_content(bs, &desc->data, &data_len);
	gf_bs_del(bs);
	desc->data_len = (u8) data_len;
	gf_list_add(stream->loop_descriptors, desc);
}

GF_EXPORT
GF_M2TS_Mux_Stream *gf_m2ts_program_stream_add(GF_M2TS_Mux_Program *program, struct __elementary_stream_ifce *ifce, u32 pid, Bool is_pcr, Bool force_pes)
{
	GF_M2TS_Mux_Stream *stream, *st;

	stream = gf_m2ts_stream_new(pid);
	stream->ifce = ifce;
	stream->pid = pid;
	stream->program = program;
	if (is_pcr) program->pcr = stream;
	stream->loop_descriptors = gf_list_new();

	if (program->streams) {
		/*if PCR keep stream at the begining*/
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
	if (program->pmt) program->pmt->table_needs_update = 1;
	stream->bit_rate = ifce->bit_rate;
	stream->scheduling_priority = 1;

	switch (ifce->stream_type) {
	case GF_STREAM_VISUAL:
		/*just pick first valid stream_id in visual range*/
		stream->mpeg2_stream_id = 0xE0;
		/*for video streams, prevent sending two frames start in one PES. This will
		introduce more overhead at very low bitrates where such cases happen, but will ensure proper timing
		of each frame*/
		stream->prevent_two_au_start_in_pes = 1;
		switch (ifce->object_type_indication) {
		case GPAC_OTI_VIDEO_MPEG4_PART2:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG4;
			break;
		case GPAC_OTI_VIDEO_AVC:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_H264;
			/*make sure we send AU delim NALU in same PES as first VCL NAL: 6 bytes (start code + 1 nal hdr + AU delim) 
			+ 4 byte start code + first nal header*/
			stream->min_bytes_copy_from_next = 11;
			break;
		case GPAC_OTI_VIDEO_SVC:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_SVC;
			/*make sure we send AU delim NALU in same PES as first VCL NAL: 6 bytes (start code + 1 nal hdr + AU delim) 
			+ 4 byte start code + first nal header*/
			stream->min_bytes_copy_from_next = 11;
			gf_m2ts_stream_add_hierarchy_descriptor(stream);
			break;	
		case GPAC_OTI_VIDEO_HEVC:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_HEVC;
			/*make sure we send AU delim NALU in same PES as first VCL NAL: 7 bytes (4 start code + 2 nal header + 1 AU delim)
			+ 4 byte start code + first nal header*/
			stream->min_bytes_copy_from_next = 12;
			break;
		case GPAC_OTI_VIDEO_SHVC:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_SHVC;
			/*make sure we send AU delim NALU in same PES as first VCL NAL: 7 bytes (4 start code + 2 nal header + 1 AU delim)
			+ 4 byte start code + first nal header*/
			stream->min_bytes_copy_from_next = 12;
			gf_m2ts_stream_add_hierarchy_descriptor(stream);
			//force by default with SHVC since we don't have any delimiter / layer yet
			stream->program->mux->one_au_per_pes = 1;
			break;
		case GPAC_OTI_VIDEO_MPEG1:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG1;
			break;
		case GPAC_OTI_VIDEO_MPEG2_SIMPLE:
		case GPAC_OTI_VIDEO_MPEG2_MAIN:
		case GPAC_OTI_VIDEO_MPEG2_SNR:
		case GPAC_OTI_VIDEO_MPEG2_SPATIAL:
		case GPAC_OTI_VIDEO_MPEG2_HIGH:
		case GPAC_OTI_VIDEO_MPEG2_422:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG2;
			break;
		/*JPEG/PNG carried in MPEG-4 PES*/
		case GPAC_OTI_IMAGE_JPEG:
		case GPAC_OTI_IMAGE_PNG:
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
			stream->mpeg2_stream_id = 0xFA;
			gf_m2ts_stream_set_default_slconfig(stream);
			break;
		default:
			break;
		}
		break;
	case GF_STREAM_AUDIO:
		switch (ifce->object_type_indication) {
		case GPAC_OTI_AUDIO_MPEG1:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_MPEG1;
			break;
		case GPAC_OTI_AUDIO_MPEG2_PART3:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_MPEG2;
			break;
		case GPAC_OTI_AUDIO_AAC_MPEG4:
		case GPAC_OTI_AUDIO_AAC_MPEG2_MP:
		case GPAC_OTI_AUDIO_AAC_MPEG2_LCP:
		case GPAC_OTI_AUDIO_AAC_MPEG2_SSRP:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_LATM_AAC;
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_AAC;
			if (!ifce->repeat_rate) ifce->repeat_rate = 500;
			break;
		case GPAC_OTI_AUDIO_AC3:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_AC3;
			break;
		}
		/*just pick first valid stream_id in audio range*/
		stream->mpeg2_stream_id = 0xC0;
		break;
	case GF_STREAM_OD:
		/*highest priority for OD streams as they are needed to process other streams*/
		stream->scheduling_priority = 20;
		stream->mpeg2_stream_id = 0xFA;
		stream->table_id = GF_M2TS_TABLE_ID_MPEG4_OD;
		gf_m2ts_stream_set_default_slconfig(stream);
		if (force_pes) {
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
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
		} else {
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_SECTIONS;
		}
		break;
	}


	if (! (ifce->caps & GF_ESI_STREAM_WITHOUT_MPEG4_SYSTEMS)) {
		/*override signaling for all streams except BIFS/OD, to use MPEG-4 PES*/
		if (program->mpeg4_signaling==GF_M2TS_MPEG4_SIGNALING_FULL) {
			if (stream->mpeg2_stream_type != GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
				stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
				stream->mpeg2_stream_id = 0xFA;/*ISO/IEC14496-1_SL-packetized_stream*/
				gf_m2ts_stream_set_default_slconfig(stream);
			}
		}
	}

	stream->ifce->output_ctrl = gf_m2ts_output_ctrl;
	stream->ifce->output_udta = stream;
	stream->mx = gf_mx_new("M2TS PID");
	if (ifce->timescale != 90000) stream->ts_scale = 90000.0 / ifce->timescale;
	return stream;
}

GF_EXPORT
GF_Err gf_m2ts_program_stream_update_ts_scale(GF_ESInterface *_self, u32 time_scale)
{
	GF_M2TS_Mux_Stream *stream = (GF_M2TS_Mux_Stream *)_self->output_udta;
	if (!stream || !time_scale)
		return GF_BAD_PARAM;
	stream->ts_scale = 90000.0 / time_scale;

	return GF_OK;
}

GF_EXPORT
GF_M2TS_Mux_Program *gf_m2ts_mux_program_add(GF_M2TS_Mux *muxer, u32 program_number, u32 pmt_pid, u32 pmt_refresh_rate, u32 pcr_offset, Bool mpeg4_signaling)
{
	GF_M2TS_Mux_Program *program;

	GF_SAFEALLOC(program, GF_M2TS_Mux_Program);
	program->mux = muxer;
	program->mpeg4_signaling = mpeg4_signaling;
	program->pcr_offset = pcr_offset;
	
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
	program->pmt->table_needs_update = 1;
	muxer->pat->table_needs_update = 1;
	program->pmt->process = gf_m2ts_stream_process_pmt;
	program->pmt->refresh_rate_ms = pmt_refresh_rate ? pmt_refresh_rate : (u32) -1;
	return program;
}

GF_EXPORT
GF_M2TS_Mux *gf_m2ts_mux_new(u32 mux_rate, u32 pat_refresh_rate, Bool real_time)
{
	GF_BitStream *bs;
	GF_M2TS_Mux *muxer;
	GF_SAFEALLOC(muxer, GF_M2TS_Mux);
	muxer->pat = gf_m2ts_stream_new(GF_M2TS_PID_PAT);
	muxer->pat->process = gf_m2ts_stream_process_pat;
	muxer->pat->refresh_rate_ms = pat_refresh_rate ? pat_refresh_rate : (u32) -1;
	muxer->real_time = real_time;
	muxer->bit_rate = mux_rate;
	muxer->init_pcr_value = 0;
	if (mux_rate) muxer->fixed_rate = 1;

	/*format NULL packet*/
	bs = gf_bs_new(muxer->null_pck, 188, GF_BITSTREAM_WRITE);
	gf_bs_write_int(bs,	0x47, 8);
	gf_bs_write_int(bs,	0, 1);
	gf_bs_write_int(bs,	0, 1);
	gf_bs_write_int(bs,	0, 1);
	gf_bs_write_int(bs,	0x1FFF, 13);
	gf_bs_write_int(bs,	0, 2);
	gf_bs_write_int(bs,	1, 2);
	gf_bs_write_int(bs,	0, 4);
	gf_bs_del(bs);
	gf_rand_init(0);
	return muxer;
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
	if (st->mx) gf_mx_del(st->mx);
	if (st->loop_descriptors) {
		while (gf_list_count(st->loop_descriptors) ) {
			GF_M2TSDescriptor *desc = gf_list_last(st->loop_descriptors);
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
	while (prog->streams) {
		GF_M2TS_Mux_Stream *st = prog->streams->next;
		gf_m2ts_mux_stream_del(prog->streams);
		prog->streams = st;
	}
	if (prog->loop_descriptors) {
		while (gf_list_count(prog->loop_descriptors) ) {
			GF_M2TSDescriptor *desc = gf_list_last(prog->loop_descriptors);
			gf_list_rem_last(prog->loop_descriptors);
			if (desc->data) gf_free(desc->data);
			gf_free(desc);
		}
		gf_list_del(prog->loop_descriptors);
	}
	gf_m2ts_mux_stream_del(prog->pmt);
	gf_free(prog);
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
	gf_free(mux);
}

void gf_m2ts_mux_update_bitrate(GF_M2TS_Mux *mux)
{
	GF_M2TS_Mux_Program *prog;
	if (!mux || mux->fixed_rate) return;

	mux->bit_rate = 0;
	gf_m2ts_mux_table_update_bitrate(mux, mux->pat);
	mux->bit_rate += mux->pat->bit_rate;


	prog = mux->programs;
	while (prog) {
		GF_M2TS_Mux_Stream *stream = prog->streams;
		gf_m2ts_mux_table_update_bitrate(mux, prog->pmt);
		mux->bit_rate += prog->pmt->bit_rate;
		while (stream) {
			mux->bit_rate += stream->bit_rate;
			stream = stream->next;
		}
		prog = prog->next;
	}
}

GF_EXPORT
void gf_m2ts_mux_update_config(GF_M2TS_Mux *mux, Bool reset_time)
{
	GF_M2TS_Mux_Program *prog;

	if (!mux->fixed_rate) {
		mux->bit_rate = 0;

		/*get PAT bitrate*/
		gf_m2ts_mux_table_update_bitrate(mux, mux->pat);
		mux->bit_rate += mux->pat->bit_rate;
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
		if (!mux->fixed_rate) {
			gf_m2ts_mux_table_update_bitrate(mux, prog->pmt);
			mux->bit_rate += prog->pmt->bit_rate;
		}
		prog = prog->next;
	}
	/*reset mux time*/
	if (reset_time) {
		mux->time.sec = mux->time.nanosec = 0;
		mux->init_sys_time = 0;
	}
}

GF_EXPORT
u32 gf_m2ts_get_sys_clock(GF_M2TS_Mux *muxer)
{
	return gf_sys_clock() - muxer->init_sys_time;
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
GF_Err gf_m2ts_mux_use_single_au_pes_mode(GF_M2TS_Mux *muxer, Bool strict_au_pes_mode)
{
	if (!muxer) return GF_BAD_PARAM;
	muxer->one_au_per_pes = strict_au_pes_mode ? 1 : 0;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_m2ts_mux_set_initial_pcr(GF_M2TS_Mux *muxer, u64 init_pcr_value)
{
	if (!muxer) return GF_BAD_PARAM;
	muxer->init_pcr_value = init_pcr_value;
	return GF_OK;
}

GF_EXPORT
const char *gf_m2ts_mux_process(GF_M2TS_Mux *muxer, u32 *status, u32 *usec_till_next)
{
	GF_M2TS_Mux_Program *program;
	GF_M2TS_Mux_Stream *stream, *stream_to_process;
	GF_M2TS_Time time;
	u32 now, nb_streams, nb_streams_done;
	char *ret;
	u32 res, highest_priority;
	Bool flush_all_pes = 0;

	nb_streams = nb_streams_done = 0;
	*status = GF_M2TS_STATE_IDLE;

	now = gf_sys_clock();
	if (muxer->real_time) {
		if (!muxer->init_sys_time) {
			//init TS time
			muxer->time.sec = muxer->time.nanosec = 0;
			gf_m2ts_time_inc(&muxer->time, (u32) muxer->init_pcr_value, 27000000);
			muxer->init_sys_time = now;
			muxer->init_ts_time = muxer->time;
		} else {
			u32 diff = now - muxer->init_sys_time;
			GF_M2TS_Time now = muxer->init_ts_time;
			gf_m2ts_time_inc(&now, diff, 1000);
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

	/*bitrate have changed*/
	if (muxer->needs_reconfig) {
		gf_m2ts_mux_update_config(muxer, 0);
		muxer->needs_reconfig = 0;
	}

	if (muxer->flush_pes_at_rap && muxer->force_pat) {
		program = muxer->programs;
		while (program) {
			stream = program->streams;
			while (stream) {
				if (stream->pes_data_remain) {
					flush_all_pes = 1;
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
				muxer->force_pat = 0;
				muxer->force_pat_pmt_state = GF_SEG_BOUNDARY_FORCE_PMT;
			}
			/*force sending the PAT regardless of other streams*/
			goto send_pck;
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
		time.sec = 0xFFFFFFFF;
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

				if (res && gf_m2ts_time_less_or_equal(&stream->time, &time)) {
					/*if same priority schedule the earliest data*/
					if (res>=highest_priority) {
						highest_priority = res;
						time = stream->time;
						stream_to_process = stream;
#if FORCE_PCR_FIRST
						goto send_pck;
#endif
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
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Inserting empty packet at %d:%d\n", time.sec, time.nanosec));
			ret = muxer->null_pck;
			muxer->tot_pad_sent++;
		}
	} else {

		if (stream_to_process->tables) {
			gf_m2ts_mux_table_get_next_packet(stream_to_process, muxer->dst_pck);
		} else {
			gf_m2ts_mux_pes_get_next_packet(stream_to_process, muxer->dst_pck);
		}

		ret = muxer->dst_pck;
		*status = GF_M2TS_STATE_DATA;

#ifndef GPAC_DISABLE_LOG
		if (gf_log_tool_level_on(GF_LOG_CONTAINER, GF_LOG_DEBUG) 
			&& muxer->fixed_rate 
		) {
			s32 drift;
			drift= muxer->time.nanosec;
			drift-=time.nanosec;
			drift/=1000000;
			if (muxer->time.sec!=time.sec) {
				drift += (muxer->time.sec - time.sec)*1000;
				assert(muxer->time.sec > time.sec);
			}
//			fprintf(stderr, "\nMux time - Packet PID %d time: %d ms\n", stream_to_process->pid, drift);
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Send %s from PID %d at %d:%09d - mux time %d:%09d\n", stream_to_process->tables ? "table" : "PES", stream_to_process->pid, time.sec, time.nanosec, muxer->time.sec, muxer->time.nanosec));
#endif


		if (nb_streams && (nb_streams==nb_streams_done)) 
			*status = GF_M2TS_STATE_EOS;
	}
	if (ret) {
		muxer->tot_pck_sent++;
		/*increment time*/
		if (muxer->fixed_rate || muxer->real_time) {
			gf_m2ts_time_inc(&muxer->time, 1504/*188*8*/, muxer->bit_rate);
		}
		/*if a stream was found, use it*/
		else if (stream_to_process) {
			muxer->time = time;
		}

		muxer->pck_sent_over_br_window++;
		if (now - muxer->last_br_time > 1000) {
			u64 size = 8*188*muxer->pck_sent_over_br_window;
			muxer->average_birate_kbps = (u32) (size /(now - muxer->last_br_time));
			muxer->last_br_time = now;
			muxer->pck_sent_over_br_window=0;
		}
	}
	return ret;
}

#endif /*GPAC_DISABLE_MPEG2TS_MUX*/

