/*
 *			GPAC - Multimedia Framework C SDK
 *
 *    Copyright (c)2008-200X Telecom ParisTech - All rights reserved
 *			Authors: Jean Le Feuvre
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

#ifndef GPAC_DISABLE_MPEG2TS_MUX


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

		gf_bs_write_data(bs, table_payload + offset, section->length - overhead_size);
		offset += section->length - overhead_size;
	
		if (use_syntax_indicator) {
			/* place holder for CRC */
			gf_bs_write_u32(bs, 0);
		}

		gf_bs_get_content(bs, (char**) &section->data, &section->length); 
		gf_bs_del(bs);

		if (use_syntax_indicator) {
			u32 CRC;
			CRC = gf_crc_32(section->data,section->length-CRC_LENGTH); 
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
			stream->bit_rate += section->length;
			section = section->next;
		}
		table = table->next;
	}
	stream->bit_rate *= 8;
	if (!stream->refresh_rate_ms) stream->refresh_rate_ms = 500;
	stream->bit_rate *= 1000;
	stream->bit_rate /= stream->refresh_rate_ms;
}

void gf_m2ts_mux_table_update_mpeg4(GF_M2TS_Mux_Stream *stream, u8 table_id, u16 table_id_extension,
						   u8 *table_payload, u32 table_payload_length, 
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
	sl_size = gf_sl_get_header_size(&stream->sl_config, &hdr);
	/*SL-packetized data doesn't fit in one section, we must repacketize*/
	if (sl_size + table_payload_length > maxSectionLength - overhead_size) {
		nb_sections = 0;
		offset = 0;
		hdr.accessUnitEndFlag = 0;
		while (offset<table_payload_length) {
			sl_size = gf_sl_get_header_size(&stream->sl_config, &hdr);
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
		gf_sl_packetize(&stream->sl_config, &hdr, NULL, 0, &slhdr, &slhdr_size);
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
		gf_bs_write_data(bs, table_payload + offset, sl_size);
		offset += sl_size;
	
		if (use_syntax_indicator) {
			/* place holder for CRC */
			gf_bs_write_u32(bs, 0);
		}

		gf_bs_get_content(bs, (char**) &section->data, &section->length); 
		gf_bs_del(bs);

		if (use_syntax_indicator) {
			u32 CRC;
			CRC = gf_crc_32(section->data,section->length-CRC_LENGTH); 
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

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Generating %d sections for MPEG-4 SL packet - version number %d - extension ID %d\n", stream->pid, nb_sections, table->version_number, table_id_extension));

	if (stream->ifce->repeat_rate) {
		stream->refresh_rate_ms = stream->ifce->repeat_rate;
		gf_m2ts_mux_table_update_bitrate(stream->program->mux, stream);
	} else {
		gf_m2ts_mux_table_update_bitrate(stream->program->mux, stream);
		stream->refresh_rate_ms=0;
	}
}

/* length of adaptation_field_length; */ 
#define ADAPTATION_LENGTH_LENGTH 1
/* discontinuty flag, random access flag ... */
#define ADAPTATION_FLAGS_LENGTH 1 
/* length of encoded pcr */
#define PCR_LENGTH 6

static u32 gf_m2ts_add_adaptation(GF_BitStream *bs, u16 pid,  
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


#ifndef GPAC_DISABLE_LOG
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Adding adaptation field size %d - RAP %d - Padding %d - PCR %d\n", pid, adaptation_length, is_rap, padding_length, pcr_time));
	} else {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Adding adaptation field size %d - RAP %d - Padding %d\n", pid, adaptation_length, is_rap, padding_length));
#endif

	}

	while (padding_length) {
		gf_bs_write_u8(bs,	0xff); // stuffing byte
		padding_length--;
	}

	return adaptation_length + ADAPTATION_LENGTH_LENGTH;
}

void gf_m2ts_mux_table_get_next_packet(GF_M2TS_Mux_Stream *stream, u8 *packet)
{
	GF_BitStream *bs;
	GF_M2TS_Mux_Table *table;
	GF_M2TS_Mux_Section *section;
	u32 payload_length, padding_length;
	u8 adaptation_field_control;

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
		gf_m2ts_add_adaptation(bs, stream->pid, 0, 0, 0, padding_length);

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

}



Bool gf_m2ts_stream_process_pat(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
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
		gf_free(payload);
	}
	return 1;
}

Bool gf_m2ts_stream_process_pmt(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
{
	if (stream->table_needs_update) { /* generate table payload */
		GF_M2TS_Mux_Stream *es;
		u8 *payload;
		u32 length, nb_streams=0;
		GF_BitStream *bs;

		bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs,	0x7, 3); // reserved
		gf_bs_write_int(bs,	stream->program->pcr->pid, 13);
		gf_bs_write_int(bs,	0xF, 4); // reserved
	
		if (!stream->program->iod) {
			gf_bs_write_int(bs,	0, 12); // program info length =0
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
						memcpy(esd->slConfig, &es_stream->sl_config, sizeof(GF_SLConfig));
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
			gf_bs_write_int(bs,	len, 12); // program info length 
			
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
		es = stream->program->streams;
		while (es) {
			nb_streams++;
			gf_bs_write_int(bs,	es->mpeg2_stream_type, 8);
			gf_bs_write_int(bs,	0x7, 3); // reserved
			gf_bs_write_int(bs,	es->pid, 13);
			gf_bs_write_int(bs,	0xF, 4); // reserved
			
			/* Second Loop Descriptor */
			if (stream->program->iod) {
				gf_bs_write_int(bs,	4, 12); // ES info length = 4 :only SL Descriptor
				gf_bs_write_int(bs,	GF_M2TS_MPEG4_SL_DESCRIPTOR, 8); 
				gf_bs_write_int(bs,	2, 8); 
				gf_bs_write_int(bs,	es->ifce->stream_id, 16);  // mpeg4_esid
			} else {
				gf_bs_write_int(bs,	0, 12);
			}
			es = es->next;
		}
	
		gf_bs_get_content(bs, (char**)&payload, &length);
		gf_bs_del(bs);

		gf_m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PMT, stream->program->number, payload, length, 1, 0, 0);
		stream->table_needs_update = 0;
		gf_free(payload);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Updating PMT - Program Number %d - %d streams - size %d%s\n", stream->pid, stream->program->number, nb_streams, length, stream->program->iod ? " - MPEG-4 Systems detected":""));
	}
	return 1;
}

static u32 gf_m2ts_stream_get_pes_header_length(GF_M2TS_Mux_Stream *stream)
{
	u32 hdr_len;
	/*not the AU start*/
	if (stream->pck_offset || !(stream->curr_pck.flags & GF_ESI_DATA_AU_START) ) return 0;
	hdr_len = 9;
	if (stream->curr_pck.flags & GF_ESI_DATA_HAS_CTS) hdr_len += 5;
	if (stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS) hdr_len += 5;
	return hdr_len;
}

Bool gf_m2ts_stream_process_stream(GF_M2TS_Mux *muxer, GF_M2TS_Mux_Stream *stream)
{
	u64 pcr_offset;
	u32 next_time;
	Bool ret = 0;

	if (stream->mpeg2_stream_type==GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
		/*section is not completely sent yet or this is not the first section of the table*/
		if (stream->current_section && (stream->current_section_offset || stream->current_section!=stream->current_table->section)) 
			return 1;
		if (stream->ifce->repeat_rate && stream->tables)
			ret = 1;
	}
	else if (stream->curr_pck.data_len && stream->pck_offset < stream->curr_pck.data_len) {
		/*PES packet not completely sent yet*/
		return 1;
	}

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
		if (stream->ifce->caps & GF_ESI_STREAM_IS_OVER) return 0;
		assert( stream->ifce->input_ctrl);
		stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_PULL, &stream->curr_pck);
	} else {
		GF_M2TS_Packet *curr_pck;

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

	/*Rescale our timestamps and express them in PCR*/
	if (stream->ts_scale) {
		stream->curr_pck.cts = (u64) (stream->ts_scale * (s64) stream->curr_pck.cts);
		stream->curr_pck.dts = (u64) (stream->ts_scale * (s64) stream->curr_pck.dts);
	}

	/*initializing the PCR*/
	if (!stream->program->pcr_init_time) {
		if (stream==stream->program->pcr) {
			while (!stream->program->pcr_init_time)
				stream->program->pcr_init_time = gf_rand();

			stream->program->pcr_init_time = 1;

			stream->program->ts_time_at_pcr_init = muxer->time;
			stream->program->num_pck_at_pcr_init = muxer->tot_pck_sent;

			GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Initializing PCR for program number %d: PCR %d - mux time %d:%09d\n", stream->pid, stream->program->number, stream->program->pcr_init_time, muxer->time.sec, muxer->time.nanosec));
		} else {
			/*don't send until PCR is initialized*/
			return 0;
		}
	}
	if (!stream->initial_ts) {
		u32 nb_bits = (u32) (muxer->tot_pck_sent - stream->program->num_pck_at_pcr_init) * 1504;
		u32 nb_ticks = 90000*nb_bits / muxer->bit_rate;
		stream->initial_ts = stream->curr_pck.dts;
		if (stream->initial_ts > nb_ticks)
			stream->initial_ts -= nb_ticks;
		else
			stream->initial_ts = 0;
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

		gf_sl_packetize(&stream->sl_config, &stream->sl_header, src_data, src_data_len, &stream->curr_pck.data, &stream->curr_pck.data_len);

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
		u32 size;
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
		gf_bs_write_data(bs, stream->curr_pck.data, stream->curr_pck.data_len);
		gf_bs_align(bs);
		gf_free(stream->curr_pck.data);
		gf_bs_get_content(bs, &stream->curr_pck.data, &stream->curr_pck.data_len);
		gf_bs_del(bs);

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
		}
		/*since we reallocated the packet data buffer, force a discard in pull mode*/
		stream->discard_data = 1;
		break;
	}


	/*compute next interesting time in TS unit: this will be DTS of next packet*/
	next_time = (u32) (stream->curr_pck.dts - stream->initial_ts);
	/*we need to take into account transmission time, eg nb packets to send the data*/
	if (next_time) {
		u32 nb_pck, bytes, nb_bits, nb_ticks;
		bytes = 184 - ADAPTATION_LENGTH_LENGTH - ADAPTATION_FLAGS_LENGTH - PCR_LENGTH;
		bytes -= gf_m2ts_stream_get_pes_header_length(stream);
		nb_pck=1;
		while (bytes<stream->curr_pck.data_len) {
			bytes+=184;
			nb_pck++;
		}
		nb_bits = nb_pck * 1504;
		nb_ticks = 90000*nb_bits / muxer->bit_rate;
		if (next_time>nb_ticks)
			next_time -= nb_ticks;
		else 
			next_time = 0;
	}

	stream->time = stream->program->ts_time_at_pcr_init;
	gf_m2ts_time_inc(&stream->time, next_time, 90000);

	/*PCR offset, in 90000 hz not in 270000000*/
	pcr_offset = stream->program->pcr_init_time/300;
	stream->curr_pck.cts = stream->curr_pck.cts - stream->initial_ts + pcr_offset;
	stream->curr_pck.dts = stream->curr_pck.dts - stream->initial_ts + pcr_offset;

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Next data schedule for %d:%09d - mux time %d:%09d\n", stream->pid, stream->time.sec, stream->time.nanosec, muxer->time.sec, muxer->time.nanosec));




	/*compute bitrate if needed*/
	if (!stream->bit_rate) {
		if (!stream->last_br_time) {
			stream->last_br_time = stream->curr_pck.dts + 1;
			stream->bytes_since_last_time = stream->curr_pck.data_len;
		} else {
			if (stream->curr_pck.dts - stream->last_br_time - 1 >= 90000) {
				u64 r = 8*stream->bytes_since_last_time;
				r*=90000;
				stream->bit_rate = (u32) (r / (stream->curr_pck.dts - stream->last_br_time - 1));
				stream->program->mux->needs_reconfig = 1;
			} else {
				stream->bytes_since_last_time += stream->curr_pck.data_len;
			}
		}
	}
	return 1;
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

u32 gf_m2ts_stream_add_pes_header(GF_BitStream *bs, GF_M2TS_Mux_Stream *stream)
{
	u64 t;
	u32 pes_len;
	Bool use_pts, use_dts;
	
	gf_bs_write_int(bs,	0x1, 24);//packet start code
	gf_bs_write_u8(bs,	stream->mpeg2_stream_id);// stream id 

	use_pts = (stream->curr_pck.flags & GF_ESI_DATA_HAS_CTS) ? 1 : 0;
	use_dts = (stream->curr_pck.flags & GF_ESI_DATA_HAS_DTS) ? 1 : 0;

	pes_len = stream->curr_pck.data_len + 3; // 3 = header size
	if (use_pts) pes_len += 5;
	if (use_dts) pes_len += 5;
	gf_bs_write_int(bs, pes_len, 16); // pes packet length
	
	gf_bs_write_int(bs, 0x2, 2); // reserved
	gf_bs_write_int(bs, 0x0, 2); // scrambling
	gf_bs_write_int(bs, 0x0, 1); // priority
	gf_bs_write_int(bs, 0x1, 1); // alignment indicator
	gf_bs_write_int(bs, 0x0, 1); // copyright
	gf_bs_write_int(bs, 0x0, 1); // original or copy
	
	gf_bs_write_int(bs, use_pts, 1);
	gf_bs_write_int(bs, use_dts, 1);
	gf_bs_write_int(bs, 0x0, 6); //6 flags = 0 (ESCR, ES_rate, DSM_trick, additional_copy, PES_CRC, PES_extension)

	gf_bs_write_int(bs, use_dts*5+use_pts*5, 8);

	if (use_pts) {
		gf_bs_write_int(bs, use_dts ? 0x3 : 0x2, 4); // reserved '0011' || '0010'
		t = ((stream->curr_pck.cts >> 30) & 0x7);
		gf_bs_write_long_int(bs, t, 3);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = ((stream->curr_pck.cts >> 15) & 0x7fff);
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = stream->curr_pck.cts & 0x7fff;
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
	}

	if (use_dts) {
		gf_bs_write_int(bs, 0x1, 4); // reserved '0001'
		t = ((stream->curr_pck.dts >> 30) & 0x7);
		gf_bs_write_long_int(bs, t, 3);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = ((stream->curr_pck.dts >> 15) & 0x7fff);
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = stream->curr_pck.dts & 0x7fff;
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG-2 TS Muxer] PID %d: Adding PES header at PCR "LLD" - has PTS %d (%d) - has DTS %d (%d)\n", stream->pid, gf_m2ts_get_pcr(stream->program)/300, use_pts, stream->curr_pck.cts, use_dts, stream->curr_pck.dts));

	return pes_len+4; // 4 = start code + stream_id
}

#define PCR_UPDATE_MS	200

void gf_m2ts_mux_pes_get_next_packet(GF_M2TS_Mux_Stream *stream, u8 *packet)
{
	GF_BitStream *bs;
	Bool is_rap, needs_pcr;
	u32 remain, adaptation_field_control, payload_length, padding_length, hdr_len;
	u32 now = gf_sys_clock();

	assert(stream->pid);
	bs = gf_bs_new(packet, 188, GF_BITSTREAM_WRITE);

	hdr_len = gf_m2ts_stream_get_pes_header_length(stream);
	remain = stream->curr_pck.data_len - stream->pck_offset;

	needs_pcr = 0;
	if (hdr_len && (stream==stream->program->pcr) ) {
		if (now > stream->program->last_sys_clock + PCR_UPDATE_MS)
			needs_pcr = 1;
	}
	adaptation_field_control = GF_M2TS_ADAPTATION_NONE;
	payload_length = 184 - hdr_len;
	padding_length = 0;

	if (needs_pcr) {
		adaptation_field_control = GF_M2TS_ADAPTATION_AND_PAYLOAD;
		/*AF headers + PCR*/
		payload_length -= 8;
	} else if (remain<184) {
		/*AF headers*/
		payload_length -= 2;
		adaptation_field_control = GF_M2TS_ADAPTATION_AND_PAYLOAD;
	}
	if (remain>=payload_length) {
		padding_length = 0;
	} else {
		padding_length = payload_length - remain; 
		payload_length -= padding_length;
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

	is_rap = (hdr_len && (stream->curr_pck.flags & GF_ESI_DATA_AU_RAP) ) ? 1 : 0;

	if (adaptation_field_control != GF_M2TS_ADAPTATION_NONE) {
		u64 pcr = 0;
		if (needs_pcr) {
			/*compute PCR*/
			u32 now = gf_sys_clock();
			pcr = gf_m2ts_get_pcr(stream->program);

//			fprintf(stdout, "PCR Diff in ms %d - sys clock diff in ms %d\n", (u32) (pcr - stream->program->last_pcr) / 27000, now - stream->program->last_sys_clock);

			stream->program->last_pcr = pcr;
			stream->program->last_sys_clock = now;
		}
		gf_m2ts_add_adaptation(bs, stream->pid, needs_pcr, pcr, is_rap, padding_length);
	}

	if (hdr_len) gf_m2ts_stream_add_pes_header(bs, stream);

	gf_bs_del(bs);

	memcpy(packet+188-payload_length, stream->curr_pck.data + stream->pck_offset, payload_length);
	stream->pck_offset += payload_length;
	
	if (stream->pck_offset == stream->curr_pck.data_len) {
		/*PES has been sent, discard internal buffer*/
		gf_free(stream->curr_pck.data);
		stream->curr_pck.data = NULL;
		stream->curr_pck.data_len = 0;
		
		GF_LOG(GF_LOG_INFO, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Done sending PES (%d bytes) from PID %d at stream time %d:%d (DTS "LLD" - PCR "LLD")\n", stream->curr_pck.data_len, stream->pid, stream->time.sec, stream->time.nanosec, stream->curr_pck.dts, gf_m2ts_get_pcr(stream->program)/300));
	}
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

static void gf_m2ts_stream_setup_slconfig(GF_M2TS_Mux_Stream *stream)
{
	stream->sl_config.tag = GF_ODF_SLC_TAG;
	stream->sl_config.useAccessUnitStartFlag = 1;
	stream->sl_config.useAccessUnitEndFlag = 1;
	stream->sl_config.useRandomAccessPointFlag = 1;
	stream->sl_config.useTimestampsFlag = 1;
}

GF_M2TS_Mux_Stream *gf_m2ts_program_stream_add(GF_M2TS_Mux_Program *program, struct __elementary_stream_ifce *ifce, u32 pid, Bool is_pcr)
{
	GF_M2TS_Mux_Stream *stream, *st;

	stream = gf_m2ts_stream_new(pid);
	stream->ifce = ifce;
	stream->pid = pid;
	stream->program = program;
	if (is_pcr) program->pcr = stream; 
	if (program->streams) {
		st = program->streams;
		while (st->next) st = st->next;
		st->next = stream;
	} else {
		program->streams = stream;
	}
	if (program->pmt) program->pmt->table_needs_update = 1;
	stream->bit_rate = ifce->bit_rate;

	switch (ifce->stream_type) {
	case GF_STREAM_VISUAL:
		/*just pick first valid stream_id in visual range*/
		stream->mpeg2_stream_id = 0xE0;
		switch (ifce->object_type_indication) {
		case GPAC_OTI_VIDEO_MPEG4_PART2:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG4;
			break;
		case GPAC_OTI_VIDEO_AVC:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_H264;
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
			gf_m2ts_stream_setup_slconfig(stream);
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
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_LATM_AAC;
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_AAC;
			if (!ifce->repeat_rate) ifce->repeat_rate = 500;
			break;
		}
		/*just pick first valid stream_id in audio range*/
		stream->mpeg2_stream_id = 0xC0;
		break;
	case GF_STREAM_SCENE:
	case GF_STREAM_OD:
		stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_SECTIONS;
		stream->mpeg2_stream_id = 0xFA;
		stream->table_id = (ifce->stream_type==GF_STREAM_OD) ? GF_M2TS_TABLE_ID_MPEG4_OD : GF_M2TS_TABLE_ID_MPEG4_BIFS;
		gf_m2ts_stream_setup_slconfig(stream);
		break;
	}

	/*override signaling for all streams except BIFS/OD, to use MPEG-4 PES*/
	if (program->mpeg4_signaling==GF_M2TS_MPEG4_SIGNALING_FULL) {
		if (stream->mpeg2_stream_type != GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
			stream->mpeg2_stream_id = 0xFA;/*ISO/IEC14496-1_SL-packetized_stream*/
			gf_m2ts_stream_setup_slconfig(stream);
		}
	}

	stream->ifce->output_ctrl = gf_m2ts_output_ctrl;
	stream->ifce->output_udta = stream;
	stream->mx = gf_mx_new("M2TS PID");
	if (ifce->timescale != 90000) stream->ts_scale = 90000.0 / ifce->timescale;
	return stream;
}

GF_Err gf_m2ts_program_stream_update_ts_scale(GF_ESInterface *_self, u32 time_scale)
{
	GF_M2TS_Mux_Stream *stream = (GF_M2TS_Mux_Stream *)_self->output_udta;
	if (!stream || !time_scale)
		return GF_BAD_PARAM;
	stream->ts_scale = 90000.0 / time_scale;

	return GF_OK;
}

void gf_m2ts_program_stream_update_sl_config(GF_ESInterface *_self, GF_SLConfig *slc)
{
	GF_M2TS_Mux_Stream *stream = (GF_M2TS_Mux_Stream *)_self->output_udta;
	if (stream->program->iod && slc) {
		memcpy(&stream->sl_config, slc, sizeof(GF_SLConfig));
	}
}

#define GF_M2TS_PSI_DEFAULT_REFRESH_RATE	200

GF_M2TS_Mux_Program *gf_m2ts_mux_program_add(GF_M2TS_Mux *muxer, u32 program_number, u32 pmt_pid, u32 pmt_refresh_rate, Bool mpeg4_signaling)
{
	GF_M2TS_Mux_Program *program;

	GF_SAFEALLOC(program, GF_M2TS_Mux_Program);
	program->mux = muxer;
	program->mpeg4_signaling = mpeg4_signaling;
	
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
	muxer->pat->table_needs_update = 1;
	program->pmt->process = gf_m2ts_stream_process_pmt;
	program->pmt->refresh_rate_ms = pmt_refresh_rate ? pmt_refresh_rate : GF_M2TS_PSI_DEFAULT_REFRESH_RATE;
	return program;
}

GF_M2TS_Mux *gf_m2ts_mux_new(u32 mux_rate, u32 pat_refresh_rate, Bool real_time)
{
	GF_BitStream *bs;
	GF_M2TS_Mux *muxer;
	GF_SAFEALLOC(muxer, GF_M2TS_Mux);
	muxer->pat = gf_m2ts_stream_new(GF_M2TS_PID_PAT);
	muxer->pat->process = gf_m2ts_stream_process_pat;
	muxer->pat->refresh_rate_ms = pat_refresh_rate ? pat_refresh_rate : GF_M2TS_PSI_DEFAULT_REFRESH_RATE;
	muxer->real_time = real_time;
	muxer->bit_rate = mux_rate;
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
	gf_free(st);
}

void gf_m2ts_mux_program_del(GF_M2TS_Mux_Program *prog)
{
	while (prog->streams) {
		GF_M2TS_Mux_Stream *st = prog->streams->next;
		gf_m2ts_mux_stream_del(prog->streams);
		prog->streams = st;
	}
	gf_m2ts_mux_stream_del(prog->pmt);
	gf_free(prog);
}

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
			/*!! WATCHOUT - this is raw bitrate without PES header overhead !!*/
			if (!mux->fixed_rate) {
				mux->bit_rate += stream->bit_rate;
				/*update PCR every 100ms - we need at least 8 bytes without padding*/
				if (stream == prog->pcr) mux->bit_rate += 8*8*10;
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

u32 gf_m2ts_get_sys_clock(GF_M2TS_Mux *muxer)
{
	return gf_sys_clock() - muxer->init_sys_time;
}

u32 gf_m2ts_get_ts_clock(GF_M2TS_Mux *muxer)
{
	u32 now, init;
	init = muxer->init_ts_time.sec*1000 + muxer->init_ts_time.nanosec/1000000;
	now = muxer->time.sec*1000 + muxer->time.nanosec/1000000;
	return now-init;
}


const char *gf_m2ts_mux_process(GF_M2TS_Mux *muxer, u32 *status)
{
	GF_M2TS_Mux_Program *program;
	GF_M2TS_Mux_Stream *stream, *stream_to_process;
	GF_M2TS_Time time;
	u32 now, nb_streams, nb_streams_done;
	char *ret;
	Bool res;

	nb_streams = nb_streams_done = 0;
	*status = GF_M2TS_STATE_IDLE;

	now = gf_sys_clock();

	if (muxer->real_time) {
		if (!muxer->init_sys_time) {
			muxer->init_sys_time = now;
			muxer->init_ts_time = muxer->time;
		} else {
			u32 diff = now - muxer->init_sys_time;
			GF_M2TS_Time now = muxer->init_ts_time;
			gf_m2ts_time_inc(&now, diff, 1000);
			if (gf_m2ts_time_less(&now, &muxer->time)) 
				return NULL;
		}
	}

	stream_to_process = NULL;
	time = muxer->time;

	if (muxer->needs_reconfig) {
		gf_m2ts_mux_update_config(muxer, 0);
		muxer->needs_reconfig = 0;
	}

	/*PAT*/
	res = muxer->pat->process(muxer, muxer->pat);
	if (res && gf_m2ts_time_less_or_equal(&muxer->pat->time, &time) ) {
		time = muxer->pat->time;
		stream_to_process = muxer->pat;
		/*force sending the PAT regardless of other streams*/
		goto send_pck;
	}

	/*PMT, for each program*/
	program = muxer->programs;
	while (program) {
		res = program->pmt->process(muxer, program->pmt);
		if (res && gf_m2ts_time_less(&program->pmt->time, &time) ) {
			time = program->pmt->time;
			stream_to_process = program->pmt;
			/*force sending the PMT regardless of other streams*/
			goto send_pck;
		}
		program = program->next;
	}

	/*PCR stream, for each program (send them first to avoid PCR to never be sent)*/
	program = muxer->programs;
	while (program) {
		stream = program->streams;
		while (stream) {
			if (stream == program->pcr) {
				nb_streams++;
				res = stream->process(muxer, stream);
				if (res) {
					if (gf_m2ts_time_less(&stream->time, &time)) {
						time = stream->time;
						stream_to_process = stream;
						goto send_pck;
					}
				} else {
					if (stream->ifce->caps & GF_ESI_STREAM_IS_OVER) nb_streams_done ++;
				}

				break;
			}
			stream = stream->next;
		}
		program = program->next;
	}

	/*all streams except PCR, for each program*/
	program = muxer->programs;
	while (program) {
		stream = program->streams;
		while (stream) {
			if (stream != program->pcr) {
				nb_streams++;
				res = stream->process(muxer, stream);
				if (res) {
					if (gf_m2ts_time_less(&stream->time, &time)) {
						time = stream->time;
						stream_to_process = stream;
						goto send_pck;
					}
				} else {
					if (stream->ifce->caps & GF_ESI_STREAM_IS_OVER) nb_streams_done ++;
				}
			}
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
		/*we still need to increase the mux time, even though we're not fixed-rate*/
		else {
			gf_m2ts_time_inc(&muxer->time, 1504/*188*8*/, muxer->bit_rate);
		}
	} else {
		if (stream_to_process->tables) {
			gf_m2ts_mux_table_get_next_packet(stream_to_process, muxer->dst_pck);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Send table from PID %d at %d:%09d - mux time %d:%09d\n", stream_to_process->pid, time.sec, time.nanosec, muxer->time.sec, muxer->time.nanosec));
		} else {
			gf_m2ts_mux_pes_get_next_packet(stream_to_process, muxer->dst_pck);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Send PES from PID %d at %d:%09d - mux time %d:%09d\n", stream_to_process->pid, time.sec, time.nanosec, muxer->time.sec, muxer->time.nanosec));
		}
		ret = muxer->dst_pck;
		*status = GF_M2TS_STATE_DATA;
	}
	if (ret) {
		muxer->tot_pck_sent++;
		/*increment time*/
		gf_m2ts_time_inc(&muxer->time, 1504/*188*8*/, muxer->bit_rate);

		if (muxer->real_time) {
			muxer->pck_sent_over_br_window++;
			if (now - muxer->last_br_time > 500) {
				u64 size = 8*188*muxer->pck_sent_over_br_window*1000;
				muxer->avg_br = (u32) (size/(now - muxer->last_br_time));
				muxer->last_br_time = now;
				muxer->pck_sent_over_br_window=0;
			}
		}
	}
	return ret;
}

#endif /*GPAC_DISABLE_MPEG2TS_MUX*/

