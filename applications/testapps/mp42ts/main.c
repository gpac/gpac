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
#include <gpac/media_tools.h>
#include <gpac/constants.h>
#include <gpac/ietf.h>
#include "mp42ts.h"

void usage() 
{
	fprintf(stderr, "usage: mp42ts [options] dst\n"
					"With options being: \n"
					"-prog=FILE     specifies an input file used for a TS service\n"
					"                * currently only supports ISO files and SDP files\n"
					"                * option can be used several times, once for each program\n"
					"-rate=R        specifies target rate in kbits/sec of the multiplex\n"
					"                If not set, transport stream will be of variable bitrate\n"
					"-mpeg4			forces usage of MPEG-4 signaling (use of IOD and SL Config)\n"
					"\n"
					"dst can be a file, an RTP or a UDP destination (unicast/multicast)\n"
		);
}

static GFINLINE void m2ts_dump_time(M2TS_Time *time, char *name)
{
	fprintf(stdout, "%s: %d%03d\n", name, time->sec, time->nanosec/1000000);
}
static GFINLINE Bool m2ts_time_less(M2TS_Time *a, M2TS_Time *b) {
	if (a->sec>b->sec) return 0;
	if (a->sec==b->sec) return (a->nanosec<b->nanosec) ? 1 : 0;
	return 1;
}
static GFINLINE Bool m2ts_time_less_or_equal(M2TS_Time *a, M2TS_Time *b) {
	if (a->sec>b->sec) return 0;
	if (a->sec==b->sec) return (a->nanosec>b->nanosec) ? 0 : 1;
	return 1;
}

static GFINLINE void m2ts_time_inc(M2TS_Time *time, u32 delta_inc_num, u32 delta_inc_den)
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
void m2ts_mux_table_update(M2TS_Mux_Stream *stream, u8 table_id, u16 table_id_extension,
						   u8 *table_payload, u32 table_payload_length, 
						   Bool use_syntax_indicator, Bool private_indicator,
						   Bool use_checksum) 
{
	u32 overhead_size;
	u32 offset;
	u32 section_number, nb_sections;
	M2TS_Mux_Table *table, *prev_table;
	u32 maxSectionLength;
	M2TS_Mux_Section *section, *prev_sec;
	GF_BitStream *bs;

	/* check if there is already a table with that id */
	prev_table = NULL;
	table = stream->tables;
	while (table) {
		if (table->table_id == table_id) {
			/* if yes, we need to flush the table and increase the version number */
			M2TS_Mux_Section *sec = table->section;
			while (sec) {
				M2TS_Mux_Section *sec2 = sec->next;
				free(sec->data);
				free(sec);
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
		GF_SAFEALLOC(table, M2TS_Mux_Table);
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
	case GF_M2TS_TABLE_ID_BAT:
		maxSectionLength = 1024; 
		break;
	case GF_M2TS_TABLE_ID_MPEG4_BIFS:
	case GF_M2TS_TABLE_ID_MPEG4_OD:
		maxSectionLength = 4096; 
		break;
	default:
		fprintf(stderr, "Cannot create sections for table with id %d\n", table_id);
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
			fprintf(stdout,"Warning: last section number for PMT shall be 0 !!\n");
		break;
	default:
		break;
	}
	
	prev_sec = NULL;
	offset = 0;
	while (offset < table_payload_length) {
		u32 remain;
		GF_SAFEALLOC(section, M2TS_Mux_Section);

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
}

void m2ts_mux_table_update_mpeg4(M2TS_Mux_Stream *stream, u8 table_id, u16 table_id_extension,
						   u8 *table_payload, u32 table_payload_length, 
						   Bool use_syntax_indicator, Bool private_indicator,
						   Bool use_checksum) 
{
	GF_SLHeader hdr;
	u32 overhead_size;
	u32 offset, sl_size;
	u32 section_number, nb_sections;
	M2TS_Mux_Table *table, *prev_table;
	/*max section length for MPEG-4 BIFS and OD*/
	u32 maxSectionLength = 4096;
	M2TS_Mux_Section *section, *prev_sec;
	GF_BitStream *bs;

	/* check if there is already a table with that id */
	prev_table = NULL;
	table = stream->tables;
	while (table) {
		if (table->table_id == table_id) {
			/* if yes, we need to flush the table and increase the version number */
			M2TS_Mux_Section *sec = table->section;
			while (sec) {
				M2TS_Mux_Section *sec2 = sec->next;
				free(sec->data);
				free(sec);
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
		GF_SAFEALLOC(table, M2TS_Mux_Table);
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
	sl_size = gf_sl_get_header_size(&stream->ifce->sl_config, &hdr);
	/*SL-packetized data doesn't fit in one section, we must repacketize*/
	if (sl_size + table_payload_length > maxSectionLength - overhead_size) {
		nb_sections = 0;
		offset = 0;
		hdr.accessUnitEndFlag = 0;
		while (offset<table_payload_length) {
			sl_size = gf_sl_get_header_size(&stream->ifce->sl_config, &hdr);
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
		GF_SAFEALLOC(section, M2TS_Mux_Section);

		hdr.accessUnitEndFlag = (section_number+1==nb_sections) ? stream->sl_header.accessUnitEndFlag : 0;
		gf_sl_packetize(&stream->ifce->sl_config, &hdr, NULL, 0, &slhdr, &slhdr_size);
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
		free(slhdr);
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
}

void m2ts_mux_table_update_bitrate(M2TS_Mux *mux, M2TS_Mux_Stream *stream)
{
	M2TS_Mux_Table *table;
	
	/*update PMT*/
	if (stream->table_needs_update) 
		stream->process(mux, stream);

	stream->bit_rate = 0;
	table = stream->tables;
	while (table) {
		M2TS_Mux_Section *section = table->section;
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

/* length of adaptation_field_length; */ 
#define ADAPTATION_LENGTH_LENGTH 1
/* discontinuty flag, random access flag ... */
#define ADAPTATION_FLAGS_LENGTH 1 
/* length of encoded pcr */
#define PCR_LENGTH 6

static u32 m2ts_add_adaptation(GF_BitStream *bs, 
				   Bool has_pcr, u64 time, 
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
		PCR_base = time;
		gf_bs_write_long_int(bs, PCR_base, 33); 
		gf_bs_write_int(bs,	0, 6); // reserved
		PCR_ext = 0;
		gf_bs_write_long_int(bs, PCR_ext, 9);
	}
	while (padding_length) {
		gf_bs_write_u8(bs,	0xff); // stuffing byte
		padding_length--;
	}

	return adaptation_length + ADAPTATION_LENGTH_LENGTH;
}

void m2ts_mux_table_get_next_packet(M2TS_Mux_Stream *stream, u8 *packet)
{
	GF_BitStream *bs;
	M2TS_Mux_Table *table;
	M2TS_Mux_Section *section;
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
		adaptation_field_control = M2TS_ADAPTATION_NONE;
	} else {		
		/* in all the following cases, we write an adaptation field */
		adaptation_field_control = M2TS_ADAPTATION_AND_PAYLOAD;
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

	if (adaptation_field_control != M2TS_ADAPTATION_NONE)
		m2ts_add_adaptation(bs, 0, 0, 0, padding_length);

	/*pointer field*/
	if (!stream->current_section_offset) {
		/* no concatenations of sections in ts packets, so start address is 0 */
		gf_bs_write_u8(bs, 0);
	}
	gf_bs_del(bs);

	memcpy(packet+188-payload_length, section->data + stream->current_section_offset, payload_length);
	stream->current_section_offset += payload_length;
	
	//m2ts_time_inc(stream->time, 1504/*188*8*/, muxer->bit_rate);

	if (stream->current_section_offset == section->length) {
		stream->current_section_offset = 0;
		stream->current_section = stream->current_section->next;
		if (!stream->current_section) {
			stream->current_table = stream->current_table->next;
			/*carousel table*/
			if (!stream->current_table && stream->refresh_rate_ms) {
				stream->current_table = stream->tables;
				/*update ES time*/
				m2ts_time_inc(&stream->time, stream->refresh_rate_ms, 1000);
			}
			if (stream->current_table) stream->current_section = stream->current_table->section;
		}
	}

}



Bool m2ts_stream_process_pat(M2TS_Mux *muxer, M2TS_Mux_Stream *stream)
{
	if (stream->table_needs_update) { /* generate table payload */
		M2TS_Mux_Program *prog;
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
		m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PAT, muxer->ts_id, payload, size, 1, 0, 0);
		stream->table_needs_update = 0;
		free(payload);
	}
	return 1;
}

Bool m2ts_stream_process_pmt(M2TS_Mux *muxer, M2TS_Mux_Stream *stream)
{
	if (stream->table_needs_update) { /* generate table payload */
		M2TS_Mux_Stream *es;
		u8 *payload;
		u32 length;
		GF_BitStream *bs;

		bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs,	0x7, 3); // reserved
		gf_bs_write_int(bs,	stream->program->pcr->pid, 13);
		gf_bs_write_int(bs,	0xF, 4); // reserved
	
		if (!stream->program->iod) {
			gf_bs_write_int(bs,	0, 12); // program info length =0
		} else {
			u32 len;
			GF_BitStream *bs_iod;
			char *iod_data;
			u32 iod_data_len;

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
				0x10 iod unique a l'intérieur de programme
				0x11 iod unoque dans le flux ts */
			gf_bs_write_int(bs,	2, 8);  

			gf_bs_write_int(bs,	2, 8);  // IOD_label
			
			gf_bs_write_data(bs, iod_data, iod_data_len);
			free(iod_data);
		}	
		es = stream->program->streams;
		while (es) {
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

		m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PMT, stream->program->number, payload, length, 1, 0, 0);
		stream->table_needs_update = 0;
		free(payload);
	}
	return 1;
}

Bool m2ts_stream_process_stream(M2TS_Mux *muxer, M2TS_Mux_Stream *stream)
{
	if (stream->mpeg2_stream_type==GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
		/*section not completely sent yet*/
		if (stream->current_section) return 1;
	} 
	else if (stream->pck_offset < stream->pck.data_len) {
		/*PES packet not completely sent yet*/
		return 1;
	}

	/*PULL mode*/
	if (stream->ifce->caps & GF_ESI_AU_PULL_CAP) {
		if (stream->pck.data_len) {
			/*discard packet data if we use SL over PES*/
			if (stream->mpeg2_stream_type==GF_M2TS_SYSTEMS_MPEG4_PES) free(stream->pck.data);
			/*release data*/
			stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_RELEASE, NULL);
		}
		stream->pck_offset = 0;
		stream->pck.data_len = 0;

		/*EOS*/
		if (stream->ifce->caps & GF_ESI_STREAM_IS_OVER) return 0;
		stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_PULL, &stream->pck);
	} else {
		M2TS_Packet *pck;
		/*flush input pipe*/
		stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_FLUSH, NULL);
		gf_mx_p(stream->mx);
		/*discard first packet*/
		if (stream->pck_offset) {
			assert(stream->pck_first);
			pck = stream->pck_first;
			stream->pck_first = pck->next;
			free(pck->data);
			free(pck);
		}
		stream->pck_offset = 0;
		stream->pck.data_len = 0;

		/*fill pck*/
		pck = stream->pck_first;
		if (!pck) {
			gf_mx_v(stream->mx);
			return 0;
		}
		stream->pck.cts = pck->cts;
		stream->pck.data = pck->data;
		stream->pck.data_len = pck->data_len;
		stream->pck.dts = pck->dts;
		stream->pck.flags = pck->flags;
		gf_mx_v(stream->mx);
	}
	if (!(stream->pck.flags & GF_ESI_DATA_HAS_DTS))
		stream->pck.dts = stream->pck.cts;

	/*!! watchout !!*/
	if (stream->ts_scale) {
		stream->pck.cts = (u64) (stream->ts_scale * (s64) stream->pck.cts);
		stream->pck.dts = (u64) (stream->ts_scale * (s64) stream->pck.dts);
	}

	/*SL-encapsultaion*/
	switch (stream->mpeg2_stream_type) {
	case GF_M2TS_SYSTEMS_MPEG4_SECTIONS:
		/*update SL config*/
		stream->sl_header.accessUnitStartFlag = (stream->pck.flags & GF_ESI_DATA_AU_START) ? 1 : 0;
		stream->sl_header.accessUnitEndFlag = (stream->pck.flags & GF_ESI_DATA_AU_END) ? 1 : 0;
		stream->sl_header.accessUnitLength += stream->pck.data_len;
		stream->sl_header.randomAccessPointFlag = (stream->pck.flags & GF_ESI_DATA_AU_RAP) ? 1: 0;
		stream->sl_header.compositionTimeStampFlag = (stream->pck.flags & GF_ESI_DATA_HAS_CTS) ? 1 : 0;
		stream->sl_header.compositionTimeStamp = stream->pck.cts;
		stream->sl_header.decodingTimeStampFlag = (stream->pck.flags & GF_ESI_DATA_HAS_DTS) ? 1: 0;
		stream->sl_header.decodingTimeStamp = stream->pck.dts;

		m2ts_mux_table_update_mpeg4(stream, stream->table_id, muxer->ts_id, stream->pck.data, stream->pck.data_len, 1, 0, 0);
		break;
	case GF_M2TS_SYSTEMS_MPEG4_PES:
	{
		char *src_data;
		u32 src_data_len;

		/*update SL config*/
		stream->sl_header.accessUnitStartFlag = (stream->pck.flags & GF_ESI_DATA_AU_START) ? 1 : 0;
		stream->sl_header.accessUnitEndFlag = (stream->pck.flags & GF_ESI_DATA_AU_END) ? 1 : 0;
		stream->sl_header.accessUnitLength += stream->pck.data_len;
		stream->sl_header.randomAccessPointFlag = (stream->pck.flags & GF_ESI_DATA_AU_RAP) ? 1: 0;
		stream->sl_header.compositionTimeStampFlag = (stream->pck.flags & GF_ESI_DATA_HAS_CTS) ? 1 : 0;
		stream->sl_header.compositionTimeStamp = stream->pck.cts;
		stream->sl_header.decodingTimeStampFlag = (stream->pck.flags & GF_ESI_DATA_HAS_DTS) ? 1: 0;
		stream->sl_header.decodingTimeStamp = stream->pck.dts;

		src_data = stream->pck.data;
		src_data_len = stream->pck.data_len;
		stream->pck.data_len = 0;
		stream->pck.data = NULL;

		gf_sl_packetize(&stream->ifce->sl_config, &stream->sl_header, src_data, src_data_len, &stream->pck.data, &stream->pck.data_len);

		/*discard src data*/
		if (!(stream->ifce->caps & GF_ESI_AU_PULL_CAP)) {
			free(src_data);
			stream->pck_first->data = stream->pck.data;
			stream->pck_first->data_len = stream->pck.data_len;
		}
	}
		break;
	}


	/*initializing the PCR*/
	if (!stream->program->pcr_init) {
		if (stream==stream->program->pcr) {
			stream->program->pcr_init_ts_time = muxer->time;
			stream->program->pcr_init_time = stream->pck.dts;
			stream->program->pcr_init = 1;
		} else {
			/*don't send until PCR is initialized*/
			return 0;
		}
	}

	/*move to current time in TS unit*/
	stream->time = stream->program->pcr_init_ts_time;
	m2ts_time_inc(&stream->time, (u32) (stream->pck.dts - stream->program->pcr_init_time), 90000);

	/*compute bitrate if needed*/
	if (!stream->bit_rate) {
		if (!stream->last_br_time) {
			stream->last_br_time = stream->pck.dts + 1;
			stream->bytes_since_last_time = stream->pck.data_len;
		} else {
			if (stream->pck.dts - stream->last_br_time - 1 >= 90000) {
				u64 r = 8*stream->bytes_since_last_time;
				r*=90000;
				stream->bit_rate = (u32) (r / (stream->pck.dts - stream->last_br_time - 1));
				stream->program->mux->needs_reconfig = 1;
			} else {
				stream->bytes_since_last_time += stream->pck.data_len;
			}
		}
	}
	return 1;
}

static u32 m2ts_stream_get_pes_header_length(M2TS_Mux_Stream *stream)
{
	u32 hdr_len;
	/*not the AU start*/
	if (stream->pck_offset || !(stream->pck.flags & GF_ESI_DATA_AU_START) ) return 0;
	hdr_len = 9;
	if (stream->pck.flags & GF_ESI_DATA_HAS_CTS) hdr_len += 5;
	if (stream->pck.flags & GF_ESI_DATA_HAS_DTS) hdr_len += 5;
	return hdr_len;
}

u32 m2ts_stream_add_pes_header(GF_BitStream *bs, M2TS_Mux_Stream *stream)
{
	u32 pes_len;
	Bool use_pts, use_dts;

	gf_bs_write_int(bs,	0x1, 24);//packet start code
	gf_bs_write_u8(bs,	stream->mpeg2_stream_id);// stream id 

	use_pts = (stream->pck.flags & GF_ESI_DATA_HAS_CTS) ? 1 : 0;
	use_dts = (stream->pck.flags & GF_ESI_DATA_HAS_DTS) ? 1 : 0;

	pes_len = stream->pck.data_len + 3; // 3 = header size
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
		u64 t;
		gf_bs_write_int(bs, use_dts ? 0x3 : 0x2, 4); // reserved '0011' || '0010'
		t = ((stream->pck.cts >> 30) & 0x7);
		gf_bs_write_long_int(bs, t, 3);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = ((stream->pck.cts >> 15) & 0x7fff);
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = stream->pck.cts & 0x7fff;
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
	}

	if (use_dts) {
		u64 t;
		gf_bs_write_int(bs, 0x1, 4); // reserved '0001'
		t = ((stream->pck.dts >> 30) & 0x7);
		gf_bs_write_long_int(bs, t, 3);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = ((stream->pck.dts >> 15) & 0x7fff);
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = stream->pck.dts & 0x7fff;
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
	}
	return pes_len+4; // 4 = start code + stream_id
}

void m2ts_mux_pes_get_next_packet(M2TS_Mux_Stream *stream, u8 *packet)
{
	GF_BitStream *bs;
	Bool is_rap, needs_pcr;
	u32 remain, adaptation_field_control, payload_length, padding_length, hdr_len;

	assert(stream->pid);
	bs = gf_bs_new(packet, 188, GF_BITSTREAM_WRITE);

	hdr_len = m2ts_stream_get_pes_header_length(stream);
	remain = stream->pck.data_len - stream->pck_offset;

	needs_pcr = (hdr_len && (stream==stream->program->pcr) ) ? 1 : 0;
	adaptation_field_control = M2TS_ADAPTATION_NONE;
	payload_length = 184 - hdr_len;
	padding_length = 0;

	if (needs_pcr) {
		adaptation_field_control = M2TS_ADAPTATION_AND_PAYLOAD;
		/*AF headers + PCR*/
		payload_length -= 8;
	} else if (remain<184) {
		/*AF headers*/
		payload_length -= 2;
		adaptation_field_control = M2TS_ADAPTATION_AND_PAYLOAD;
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

	is_rap = (hdr_len && (stream->pck.flags & GF_ESI_DATA_AU_RAP) ) ? 1 : 0;

	if (adaptation_field_control != M2TS_ADAPTATION_NONE) {
		/*FIXME - WE NEED A REAL PCR, NOT THE DTS*/
		m2ts_add_adaptation(bs, needs_pcr, stream->pck.dts, is_rap, padding_length);
	}

	/*FIXME - we need proper packetization here in case we're not fed with full AUs*/
	if (hdr_len) m2ts_stream_add_pes_header(bs, stream);

	gf_bs_del(bs);

	memcpy(packet+188-payload_length, stream->pck.data + stream->pck_offset, payload_length);
	stream->pck_offset += payload_length;
	
	assert(stream->pck_offset <= stream->pck.data_len);
	m2ts_time_inc(&stream->time, payload_length, stream->bit_rate);
}


M2TS_Mux_Stream *m2ts_stream_new(u32 pid) {
	M2TS_Mux_Stream *stream;

	GF_SAFEALLOC(stream, M2TS_Mux_Stream);
	stream->pid = pid;
	stream->process = m2ts_stream_process_stream;

	return stream;
}

GF_Err m2ts_output_ctrl(GF_ESInterface *_self, u32 ctrl_type, void *param)
{
	GF_ESIPacket *esi_pck;
	M2TS_Packet *pck;
	M2TS_Mux_Stream *stream = (M2TS_Mux_Stream *)_self->output_udta;
	switch (ctrl_type) {
	case GF_ESI_OUTPUT_DATA_DISPATCH:
		GF_SAFEALLOC(pck, M2TS_Packet);
		esi_pck = (GF_ESIPacket *)param;
		pck->data_len = esi_pck->data_len;
		pck->data = malloc(sizeof(char)*pck->data_len);
		memcpy(pck->data, esi_pck->data, pck->data_len);
		pck->flags = esi_pck->flags;
		pck->cts = esi_pck->cts;
		pck->dts = esi_pck->dts;
		gf_mx_p(stream->mx);
		if (!stream->pck_first) {
			stream->pck_first = stream->pck_last = pck;
		} else {
			stream->pck_last->next = pck;
			stream->pck_last = pck;
		}
		gf_mx_v(stream->mx);
		break;
	}
	return GF_OK;
}



M2TS_Mux_Stream *m2ts_program_stream_add(M2TS_Mux_Program *program, struct __elementary_stream_ifce *ifce, u32 pid, Bool is_pcr)
{
	M2TS_Mux_Stream *stream, *st;

	stream = m2ts_stream_new(pid);
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
		case 0x20:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG4;
			break;
		case 0x21:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_H264;
			break;
		case 0x6A:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG1;
			break;
		case 0x60:
		case 0x61:
		case 0x62:
		case 0x63:
		case 0x64:
		case 0x65:
			stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG2;
			break;
		/*JPEG/PNG carried in MPEG-4 PES*/
		case 0x6C:
		case 0x6D:
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
			stream->mpeg2_stream_id = 0xFA;
			break;
		default:
			break;
		}
		break;
	case GF_STREAM_AUDIO:
		switch (ifce->object_type_indication) {
		case 0x6B:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_MPEG1;
			break;
		case 0x69:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_MPEG2;
			break;
		case 0x40:
			stream->mpeg2_stream_type = GF_M2TS_AUDIO_AAC;
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
		break;
	}

	/*override signaling for all streams except BIFS/OD, to use MPEG-4 PES*/
	if (program->mux->mpeg4_signaling) {
		if (stream->mpeg2_stream_type != GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
			stream->mpeg2_stream_type = GF_M2TS_SYSTEMS_MPEG4_PES;
			stream->mpeg2_stream_id = 0xFA;/*ISO/IEC14496-1_SL-packetized_stream*/
		}
	}

	stream->ifce->output_ctrl = m2ts_output_ctrl;
	stream->ifce->output_udta = stream;
	stream->mx = gf_mx_new("M2TS PID");
	if (ifce->timescale != 90000) stream->ts_scale = 90000.0 / ifce->timescale;
	return stream;
}

#define M2TS_PSI_REFRESH_RATE	200

M2TS_Mux_Program *m2ts_mux_program_add(M2TS_Mux *muxer, u32 program_number, u32 pmt_pid)
{
	M2TS_Mux_Program *program;

	GF_SAFEALLOC(program, M2TS_Mux_Program);
	program->mux = muxer;
	program->number = program_number;
	if (muxer->programs) {
		M2TS_Mux_Program *p = muxer->programs;
		while (p->next) p = p->next;
		p->next = program;
	} else {
		muxer->programs = program;
	}
	program->pmt = m2ts_stream_new(pmt_pid);
	program->pmt->program = program;
	muxer->pat->table_needs_update = 1;
	program->pmt->process = m2ts_stream_process_pmt;
	program->pmt->refresh_rate_ms = M2TS_PSI_REFRESH_RATE;
	return program;
}

M2TS_Mux *m2ts_mux_new(u32 mux_rate, Bool real_time)
{
	GF_BitStream *bs;
	M2TS_Mux *muxer;
	GF_SAFEALLOC(muxer, M2TS_Mux);
	muxer->pat = m2ts_stream_new(GF_M2TS_PID_PAT);
	muxer->pat->process = m2ts_stream_process_pat;
	muxer->pat->refresh_rate_ms = M2TS_PSI_REFRESH_RATE;
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
	return muxer;
}

void m2ts_mux_stream_del(M2TS_Mux_Stream *st)
{
	while (st->tables) {
		M2TS_Mux_Table *tab = st->tables->next;
		while (st->tables->section) {
			M2TS_Mux_Section *sec = st->tables->section->next;
			free(st->tables->section->data);
			free(st->tables->section);
			st->tables->section = sec;
		}
		free(st->tables);
		st->tables = tab;
	}
	while (st->pck_first) {
		M2TS_Packet *pck = st->pck_first;
		st->pck_first = pck->next;
		free(pck->data);
		free(pck);
	}
	if (st->mx) gf_mx_del(st->mx);
	free(st);
}

void m2ts_mux_program_del(M2TS_Mux_Program *prog)
{
	while (prog->streams) {
		M2TS_Mux_Stream *st = prog->streams->next;
		m2ts_mux_stream_del(prog->streams);
		prog->streams = st;
	}
	m2ts_mux_stream_del(prog->pmt);
	free(prog);
}

void m2ts_mux_del(M2TS_Mux *mux)
{
	while (mux->programs) {
		M2TS_Mux_Program *p = mux->programs->next;
		m2ts_mux_program_del(mux->programs);
		mux->programs = p;
	}
	m2ts_mux_stream_del(mux->pat);
	free(mux);
}

void m2ts_mux_update_config(M2TS_Mux *mux, Bool reset_time)
{
	M2TS_Mux_Program *prog;

	if (!mux->fixed_rate) {
		mux->bit_rate = 0;

		/*get PAT bitrate*/
		m2ts_mux_table_update_bitrate(mux, mux->pat);
		mux->bit_rate += mux->pat->bit_rate;
	}

	prog = mux->programs;
	while (prog) {
		M2TS_Mux_Stream *stream = prog->streams;
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
			m2ts_mux_table_update_bitrate(mux, prog->pmt);
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

u32 gf_m2ts_get_sys_clock(M2TS_Mux *muxer)
{
	return gf_sys_clock() - muxer->init_sys_time;
}
u32 gf_m2ts_get_ts_clock(M2TS_Mux *muxer)
{
	u32 now, init;
	init = muxer->init_ts_time.sec*1000 + muxer->init_ts_time.nanosec/1000000;
	now = muxer->time.sec*1000 + muxer->time.nanosec/1000000;
	return now-init;
}


const char *m2ts_mux_process(M2TS_Mux *muxer, u32 *status)
{
	M2TS_Mux_Program *program;
	M2TS_Mux_Stream *stream, *stream_to_process;
	M2TS_Time time;
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
			M2TS_Time now = muxer->init_ts_time;
			m2ts_time_inc(&now, diff, 1000);

			if (m2ts_time_less(&now, &muxer->time)) 
				return NULL;
		}
	}

	stream_to_process = NULL;
	time = muxer->time;

	if (muxer->needs_reconfig) {
		m2ts_mux_update_config(muxer, 0);
		muxer->needs_reconfig = 0;
	}

	res = muxer->pat->process(muxer, muxer->pat);
	if (res && m2ts_time_less_or_equal(&muxer->pat->time, &time) ) {
		time = muxer->pat->time;
		stream_to_process = muxer->pat;
		/*force sending the PAT regardless of other streams*/
		goto send_pck;
	}

	program = muxer->programs;
	while (program) {
		res = program->pmt->process(muxer, program->pmt);
		if (res && m2ts_time_less(&program->pmt->time, &time) ) {
			time = program->pmt->time;
			stream_to_process = program->pmt;
			/*force sending the PMT regardless of other streams*/
			goto send_pck;
		}
		stream = program->streams;
		while (stream) {
			nb_streams ++;
			res = stream->process(muxer, stream);
			if (res) {
				if (m2ts_time_less(&stream->time, &time)) {
					time = stream->time;
					stream_to_process = stream;
				}
			} else {
				if (stream->ifce->caps & GF_ESI_STREAM_IS_OVER) nb_streams_done ++;
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
			m2ts_time_inc(&muxer->time, 1504/*188*8*/, muxer->bit_rate);
		}
	} else {
		if (stream_to_process->tables) {
			m2ts_mux_table_get_next_packet(stream_to_process, muxer->dst_pck);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sending table packet from PID %d at %d:%d\n", stream_to_process->pid, time.sec, time.nanosec));
		} else {
			m2ts_mux_pes_get_next_packet(stream_to_process, muxer->dst_pck);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sending PES packet from PID %d at %d:%d\n", stream_to_process->pid, time.sec, time.nanosec));
		}
		ret = muxer->dst_pck;
		*status = GF_M2TS_STATE_DATA;
	}
	if (ret) {
		muxer->tot_pck_sent++;
		/*increment time*/
		m2ts_time_inc(&muxer->time, 1504/*188*8*/, muxer->bit_rate);

		if (muxer->real_time) {
			muxer->pck_sent++;
			if (now - muxer->last_br_time > 500) {
				u64 size = 8*188*muxer->pck_sent*1000;
				muxer->avg_br = (u32) (size/(now - muxer->last_br_time));
				muxer->last_br_time = now;
				muxer->pck_sent=0;
			}
		}
	}
	return ret;
}

typedef struct
{
	GF_ISOFile *mp4;
	u32 track, sample_number, sample_count;
	GF_ISOSample *sample;
	/*refresh rate for images*/
	u32 refresh_rate_ms, nb_repeat_last;
} GF_ESIMP4;

static GF_Err mp4_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	GF_ESIMP4 *priv = (GF_ESIMP4 *)ifce->input_udta;
	if (!priv) return GF_BAD_PARAM;

	switch (act_type) {
	case GF_ESI_INPUT_DATA_FLUSH:
		return GF_OK;
	case GF_ESI_INPUT_DATA_PULL:
	{
		GF_ESIPacket *pck = (GF_ESIPacket *)param;
		if (!priv->sample) {
			priv->sample = gf_isom_get_sample(priv->mp4, priv->track, priv->sample_number+1, NULL);
		}
		if (!priv->sample) return GF_IO_ERR;
		pck->data_len = priv->sample->dataLength;
		pck->data = priv->sample->data;
		pck->flags = GF_ESI_DATA_AU_START | GF_ESI_DATA_AU_END | GF_ESI_DATA_HAS_CTS;
		pck->cts = priv->sample->DTS;
		
		if (priv->nb_repeat_last) {
			pck->cts += priv->nb_repeat_last*ifce->timescale * priv->refresh_rate_ms / 1000;
		}

		if (priv->sample->CTS_Offset) {
			pck->dts = pck->cts;
			pck->cts += priv->sample->CTS_Offset;
			pck->flags |= GF_ESI_DATA_HAS_DTS;
		}
		if (priv->sample->IsRAP) pck->flags |= GF_ESI_DATA_AU_RAP;
	}
		return GF_OK;
	case GF_ESI_INPUT_DATA_RELEASE:
		if (priv->sample) {
			gf_isom_sample_del(&priv->sample);
			priv->sample_number++;
			if (priv->sample_number==priv->sample_count) {
				if (priv->refresh_rate_ms) {
					priv->nb_repeat_last++;
					priv->sample_number--;
				} else {
					ifce->caps |= GF_ESI_STREAM_IS_OVER;
				}
			}
		}
		return GF_OK;
	case GF_ESI_INPUT_DESTROY:
		free(priv);
		ifce->input_udta = NULL;
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static void fill_isom_es_ifce(GF_ESInterface *ifce, GF_ISOFile *mp4, u32 track_num)
{
	GF_ESIMP4 *priv;
	char _lan[4];
	GF_DecoderConfig *dcd;
	u64 avg_rate;

	GF_SAFEALLOC(priv, GF_ESIMP4);

	priv->mp4 = mp4;
	priv->track = track_num;
	priv->sample_count = gf_isom_get_sample_count(mp4, track_num);

	memset(ifce, 0, sizeof(GF_ESInterface));
	ifce->caps = GF_ESI_AU_PULL_CAP;
	ifce->stream_id = gf_isom_get_track_id(mp4, track_num);
	dcd = gf_isom_get_decoder_config(mp4, track_num, 1);
	ifce->stream_type = dcd->streamType;
	ifce->object_type_indication = dcd->objectTypeIndication;
	gf_odf_desc_del((GF_Descriptor *)dcd);
	gf_isom_get_media_language(mp4, track_num, _lan);
	ifce->lang = GF_4CC(_lan[0],_lan[1],_lan[2],' ');

	ifce->timescale = gf_isom_get_media_timescale(mp4, track_num);
	ifce->duration = gf_isom_get_media_timescale(mp4, track_num);
	avg_rate = gf_isom_get_media_data_size(mp4, track_num);
	avg_rate *= ifce->timescale * 8;
	avg_rate /= gf_isom_get_media_duration(mp4, track_num);

	if (gf_isom_has_time_offset(mp4, track_num)) ifce->caps |= GF_ESI_SIGNAL_DTS;

	/*turn on image repeat*/
	switch (ifce->object_type_indication) {
	case 0x6C:
	case 0x6D:
		//priv->refresh_rate_ms = 500;
		break;
	}
	ifce->bit_rate = (u32) avg_rate;
	ifce->duration = (Double) (s64) gf_isom_get_media_duration(mp4, track_num);
	ifce->duration /= ifce->timescale;

	ifce->input_ctrl = mp4_input_ctrl;
	ifce->input_udta = priv;
}

typedef struct
{
	/*RTP channel*/
	GF_RTPChannel *rtp_ch;

	/*depacketizer*/
	GF_RTPDepacketizer *depacketizer;

	GF_ESIPacket pck;

	GF_ESInterface *ifce;
} GF_ESIRTP;

static GF_Err rtp_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	u32 size, PayloadStart;
	GF_Err e;
	GF_RTPHeader hdr;
	char buffer[8000];
	GF_ESIRTP *rtp = (GF_ESIRTP*)ifce->input_udta;

	if (!ifce->input_udta) return GF_BAD_PARAM;

	switch (act_type) {
	case GF_ESI_INPUT_DATA_FLUSH:
		/*flush rtp channel*/
		while (1) {
			size = gf_rtp_read_rtp(rtp->rtp_ch, buffer, 8000);
			if (!size) break;
			e = gf_rtp_decode_rtp(rtp->rtp_ch, buffer, size, &hdr, &PayloadStart);
			if (e) return e;
			gf_rtp_depacketizer_process(rtp->depacketizer, &hdr, buffer + PayloadStart, size - PayloadStart);
		}
		/*flush rtcp channel*/
		while (1) {
			size = gf_rtp_read_rtcp(rtp->rtp_ch, buffer, 8000);
			if (!size) break;
			e = gf_rtp_decode_rtcp(rtp->rtp_ch, buffer, size);
			if (e == GF_EOS) ifce->caps |= GF_ESI_STREAM_IS_OVER;
		}
		return GF_OK;
	case GF_ESI_INPUT_DESTROY:
		gf_rtp_depacketizer_del(rtp->depacketizer);
		gf_rtp_del(rtp->rtp_ch);
		free(rtp);
		ifce->input_udta = NULL;
		return GF_OK;
	}
	return GF_OK;
}

static void rtp_sl_packet_cbk(void *udta, char *payload, u32 size, GF_SLHeader *hdr, GF_Err e)
{
	GF_ESIRTP *rtp = (GF_ESIRTP*)udta;
	rtp->pck.data = payload;
	rtp->pck.data_len = size;
	rtp->pck.dts = hdr->decodingTimeStamp;
	rtp->pck.cts = hdr->compositionTimeStamp;
	rtp->pck.flags = 0;
	if (hdr->compositionTimeStampFlag) rtp->pck.flags |= GF_ESI_DATA_HAS_CTS;
	if (hdr->decodingTimeStampFlag) rtp->pck.flags |= GF_ESI_DATA_HAS_DTS;
	if (hdr->randomAccessPointFlag) rtp->pck.flags |= GF_ESI_DATA_AU_RAP;
	if (hdr->accessUnitStartFlag) rtp->pck.flags |= GF_ESI_DATA_AU_START;
	if (hdr->accessUnitEndFlag) rtp->pck.flags |= GF_ESI_DATA_AU_END;
	rtp->ifce->output_ctrl(rtp->ifce, GF_ESI_OUTPUT_DATA_DISPATCH, &rtp->pck);
}

void fill_rtp_es_ifce(GF_ESInterface *ifce, GF_SDPMedia *media, GF_SDPInfo *sdp)
{
	u32 i;
	GF_Err e;
	GF_X_Attribute*att;
	GF_ESIRTP *rtp;
	GF_RTPMap*map;
	GF_SDPConnection *conn;
	GF_RTSPTransport trans;

	/*check connection*/
	conn = sdp->c_connection;
	if (!conn) conn = (GF_SDPConnection*)gf_list_get(media->Connections, 0);

	/*check payload type*/
	map = (GF_RTPMap*)gf_list_get(media->RTPMaps, 0);
	GF_SAFEALLOC(rtp, GF_ESIRTP);

	memset(ifce, 0, sizeof(GF_ESInterface));
	rtp->rtp_ch = gf_rtp_new();
	i=0;
	while ((att = (GF_X_Attribute*)gf_list_enum(media->Attributes, &i))) {
		if (!stricmp(att->Name, "mpeg4-esid") && att->Value) ifce->stream_id = atoi(att->Value);
	}

	memset(&trans, 0, sizeof(GF_RTSPTransport));
	trans.Profile = media->Profile;
	trans.source = conn ? conn->host : sdp->o_address;
	trans.IsUnicast = gf_sk_is_multicast_address(trans.source) ? 0 : 1;
	if (!trans.IsUnicast) {
		trans.port_first = media->PortNumber;
		trans.port_last = media->PortNumber + 1;
		trans.TTL = conn ? conn->TTL : 0;
	} else {
		trans.client_port_first = media->PortNumber;
		trans.client_port_last = media->PortNumber + 1;
	}

	if (gf_rtp_setup_transport(rtp->rtp_ch, &trans, NULL) != GF_OK) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stdout, "Cannot initialize RTP transport\n");
		return;
	}
	/*setup depacketizer*/
	rtp->depacketizer = gf_rtp_depacketizer_new(media, rtp_sl_packet_cbk, rtp);
	if (!rtp->depacketizer) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stdout, "Cannot create RTP depacketizer\n");
		return;
	}
	/*setup channel*/
	gf_rtp_setup_payload(rtp->rtp_ch, map);
	ifce->input_udta = rtp;
	ifce->input_ctrl = rtp_input_ctrl;
	rtp->ifce = ifce;

	ifce->object_type_indication = rtp->depacketizer->sl_map.ObjectTypeIndication;
	ifce->stream_type = rtp->depacketizer->sl_map.StreamType;
	ifce->timescale = gf_rtp_get_clockrate(rtp->rtp_ch);

	/*DTS signaling is only supported for MPEG-4 visual*/
	if (rtp->depacketizer->sl_map.DTSDeltaLength) ifce->caps |= GF_ESI_SIGNAL_DTS;

	gf_rtp_depacketizer_reset(rtp->depacketizer, 1);
	e = gf_rtp_initialize(rtp->rtp_ch, 0x100000ul, 0, 0, 10, 200, NULL);
	if (e!=GF_OK) {
		gf_rtp_del(rtp->rtp_ch);
		fprintf(stdout, "Cannot initialize RTP channel: %s\n", gf_error_to_string(e));
		return;
	}
	fprintf(stdout, "RTP interface initialized\n");
}

#define MAX_MUX_SRC_PROG	100
typedef struct
{
	GF_ISOFile *mp4;
	u32 nb_streams, pcr_idx;
	GF_ESInterface streams[40];
	GF_Descriptor *iod;
} M2TSProgram;

Bool open_program(M2TSProgram *prog, const char *src)
{
	GF_SDPInfo *sdp;
	u32 i;
	GF_Err e;

	memset(prog, 0, sizeof(M2TSProgram));

	/* Open ISO file  */
	if (gf_isom_probe_file(src)) {
		u32 nb_tracks;
		u32 first_audio = 0;
		prog->mp4 = gf_isom_open(src, GF_ISOM_OPEN_READ, 0);
		prog->nb_streams = 0;
		/*on MPEG-2 TS, carry 3GPP timed text as MPEG-4 Part17*/
		gf_isom_text_set_streaming_mode(prog->mp4, 1);
		nb_tracks = gf_isom_get_track_count(prog->mp4); 
		for (i=0; i<nb_tracks; i++) {
			if (gf_isom_get_media_type(prog->mp4, i+1) == GF_ISOM_MEDIA_HINT) 
				continue; 
			fill_isom_es_ifce(&prog->streams[i], prog->mp4, i+1);
			prog->nb_streams++;
			/*get first visual stream as PCR*/
			if (!prog->pcr_idx && 
				(gf_isom_get_media_type(prog->mp4, i+1) == GF_ISOM_MEDIA_VISUAL) && 
				(gf_isom_get_sample_count(prog->mp4, i+1)>1) ) {
				prog->pcr_idx = i+1;
			}
			if (!first_audio && (gf_isom_get_media_type(prog->mp4, i+1) == GF_ISOM_MEDIA_AUDIO) ) {
				first_audio = i+1;
			}
			prog->streams[i].sl_config.timestampResolution = 90000; //prog->streams[i].timescale;
			prog->streams[i].sl_config.useRandomAccessPointFlag = 1;
			prog->streams[i].sl_config.useAccessUnitStartFlag = 1;
			prog->streams[i].sl_config.useAccessUnitEndFlag = 1;
			prog->streams[i].sl_config.useTimestampsFlag = 1;
			prog->streams[i].sl_config.timestampLength = 33;
			prog->streams[i].sl_config.tag = GF_ODF_SLC_TAG;
			gf_isom_set_extraction_slc(prog->mp4, i+1, 1, &prog->streams[i].sl_config);
		}
		/* WARNING: the returned IOD may be different from the one actually in the file
		 because it rewrites the SL config according to SL extraction settings. */
		prog->iod = gf_isom_get_root_od(prog->mp4);
		/*if no visual PCR found, use first audio*/
		if (!prog->pcr_idx) prog->pcr_idx = first_audio;
		if (prog->pcr_idx) prog->pcr_idx-=1;
		return 1;
	}

	/*open SDP file*/
	if (strstr(src, ".sdp")) {
		char *sdp_buf;
		u32 sdp_size;
		FILE *_sdp = fopen(src, "rt");
		if (!_sdp) {
			fprintf(stderr, "Error opening %s - no such file\n", src);
			return 0;
		}
		fseek(_sdp, 0, SEEK_END);
		sdp_size = ftell(_sdp);
		fseek(_sdp, 0, SEEK_SET);
		sdp_buf = (char*)malloc(sizeof(char)*sdp_size);
		memset(sdp_buf, 0, sizeof(char)*sdp_size);
		fread(sdp_buf, sdp_size, 1, _sdp);
		fclose(_sdp);

		sdp = gf_sdp_info_new();
		e = gf_sdp_info_parse(sdp, sdp_buf, sdp_size);
		free(sdp_buf);
		if (e) {
			fprintf(stderr, "Error opening %s : %s\n", src, gf_error_to_string(e));
			gf_sdp_info_del(sdp);
			return 0;
		}
		prog->nb_streams = gf_list_count(sdp->media_desc);
		for (i=0; i<prog->nb_streams; i++) {
			GF_SDPMedia *media = gf_list_get(sdp->media_desc, i);
			fill_rtp_es_ifce(&prog->streams[i], media, sdp);
			if (!prog->pcr_idx && (prog->streams[i].stream_type == GF_STREAM_VISUAL)) {
				prog->pcr_idx = i+1;
			}
		}
		if (prog->pcr_idx) prog->pcr_idx-=1;
		gf_sdp_info_del(sdp);
		return 2;
	} else {
		fprintf(stderr, "Error opening %s - not a supported input media, skipping.\n", src);
		return 0;
	}
}

int main(int argc, char **argv)
{
	const char *ts_pck;
	GF_Err e;
	u32 res, run_time;
	Bool real_time, mpeg4_signaling;
	M2TS_Mux *muxer;
	u32 i, j, mux_rate, nb_progs, cur_pid;
	char *ts_out = NULL;
FILE *ts_file;
	GF_Socket *ts_udp;
	GF_RTPChannel *ts_rtp;
	GF_RTSPTransport tr;
	GF_RTPHeader hdr;
	u16 port = 1234;
	u32 output_type;
	M2TSProgram progs[MAX_MUX_SRC_PROG];

	real_time=0;	
	output_type = 0;
	ts_file = NULL;
	ts_udp = NULL;
	ts_rtp = NULL;
	ts_out = NULL;
	nb_progs = 0;
	mux_rate = 0;
	run_time = 0;
	mpeg4_signaling = 0;
	for (i=1; i<argc; i++) {
		char *arg = argv[i];
		if (arg[0]=='-') {
			if (!strnicmp(arg, "-rate=", 6)) {
				mux_rate = 1024*atoi(arg+6);
			} else if (!strnicmp(arg, "-prog=", 6)) {
				memset(&progs[nb_progs], 0, sizeof(M2TSProgram));
				res = open_program(&progs[nb_progs], arg+6);
				if (res) {
					nb_progs++;
					if (res==2) real_time=1;
				}
			} else if (!strnicmp(arg, "-mpeg4", 6)) {
				mpeg4_signaling = 1;
			} else if (!strnicmp(arg, "-time=", 6)) {
				run_time = atoi(arg+6);
			} 
		} 
		/*output*/
		else {
			if (!strnicmp(arg, "rtp://", 6) || !strnicmp(arg, "udp://", 6)) {
				char *sep = strchr(arg+6, ':');
				output_type = (arg[0]=='r') ? 1 : 2;
				real_time=1;
				if (sep) {
					port = atoi(sep+1);
					sep[0]=0;
					ts_out = strdup(arg);
					sep[0]=':';
				} else {
					ts_out = strdup(arg+6);
				}
			} 
			else if (!strnicmp(arg, "udp://", 6)) {
				output_type = 1;
				ts_out = arg;
				real_time=1;
			} else {
				output_type = 0;
				ts_out = strdup(arg);
			}
		}
	}
	if (!nb_progs || !ts_out) {
		usage();
		return 0;
	}
	
	if (run_time && !mux_rate) {
		fprintf(stdout, "Cannot specify TS run time for VBR multiplex - disabling run time\n");
		run_time = 0;
	}

	gf_log_set_level(GF_LOG_DEBUG);
	gf_log_set_tools(GF_LOG_RTP);
//	gf_log_set_tools(GF_LOG_CONTAINER);

	gf_sys_init();

	muxer = m2ts_mux_new(mux_rate, real_time);
	muxer->mpeg4_signaling = mpeg4_signaling;
	/* Open mpeg2ts file*/
	switch(output_type) {
	case 0:
		ts_file = fopen(ts_out, "wb");
		if (!ts_file ) {
			fprintf(stderr, "Error opening %s\n", ts_out);
			goto exit;
		}
		break;
	case 1:
		ts_udp = gf_sk_new(GF_SOCK_TYPE_UDP);
		if (gf_sk_is_multicast_address((char *)ts_out)) {
			e = gf_sk_setup_multicast(ts_udp, (char *)ts_out, port, 0, 0, NULL);
		} else {
			e = gf_sk_bind(ts_udp, NULL, port, (char *)ts_out, port, GF_SOCK_REUSE_PORT);
		}
		if (e) {
			fprintf(stdout, "Error inhitializing UDP socket: %s\n", gf_error_to_string(e));
			goto exit;
		}
		break;
	case 2:
		ts_rtp = gf_rtp_new();
		gf_rtp_set_ports(ts_rtp, port);
		tr.IsUnicast = gf_sk_is_multicast_address((char *)ts_out) ? 0 : 1;
		tr.Profile="RTP/AVP";
		tr.destination = (char *)ts_out;
		tr.source = "0.0.0.0";
		tr.IsRecord = 0;
		tr.Append = 0;
		tr.SSRC = rand();
		tr.port_first = port;
		tr.port_last = port+1;
		if (tr.IsUnicast) {
			tr.client_port_first = port;
			tr.client_port_last = port+1;
		} else {
			tr.source = (char *)ts_out;
		}
		res = gf_rtp_setup_transport(ts_rtp, &tr, (char *)ts_out);
		if (res !=0) {
			fprintf(stdout, "Cannot setup RTP transport info\n");
			goto exit;
		}
		res = gf_rtp_initialize(ts_rtp, 0, 1, 1500, 0, 0, NULL);
		if (res !=0) {
			fprintf(stdout, "Cannot initialize RTP sockets\n");
			goto exit;
		}
		memset(&hdr, 0, sizeof(GF_RTPHeader));
		hdr.Version = 2;
		hdr.PayloadType = 33;	/*MP2T*/
		hdr.SSRC = tr.SSRC;
		hdr.Marker = 0;
		break;
	}
	
	cur_pid = 100;
	for (i=0; i<nb_progs; i++) {
		M2TS_Mux_Program *program = m2ts_mux_program_add(muxer, i+1, cur_pid);
		if (muxer->mpeg4_signaling) program->iod = progs[i].iod;
		for (j=0; j<progs[i].nb_streams; j++) {
			m2ts_program_stream_add(program, &progs[i].streams[j], cur_pid+j+1, (progs[i].pcr_idx==j) ? 1 : 0);
		}
		cur_pid += progs[i].nb_streams;
		while (cur_pid % 10)
			cur_pid ++;
	}

	m2ts_mux_update_config(muxer, 1);

	while (1) {
		u32 ts;
		u32 status;
		/*flush all packets*/
		switch (output_type) {
		case 0:
			while ((ts_pck = m2ts_mux_process(muxer, &status)) != NULL) {
				fwrite(ts_pck, 1, 188, ts_file); 
				if (status>=GF_M2TS_STATE_PADDING) break;
			}
			break;
		case 1:
			while ((ts_pck = m2ts_mux_process(muxer, &status)) != NULL) {
				e = gf_sk_send(ts_udp, (char*)ts_pck, 188); 
				if (e) 
					fprintf(stdout, "Error %s sending UDP packet\n", gf_error_to_string(e));
				if (status>=GF_M2TS_STATE_PADDING) break;
			}
			break;
		case 2:
			while ((ts_pck = m2ts_mux_process(muxer, &status)) != NULL) {
				hdr.SequenceNumber++;
				/*muxer clock at 90k*/
				ts = muxer->time.sec*90000 + muxer->time.nanosec*9/100000;
				/*FIXME - better discontinuity check*/
				hdr.Marker = (ts < hdr.TimeStamp) ? 1 : 0;
				hdr.TimeStamp = ts;
				e = gf_rtp_send_packet(ts_rtp, &hdr, 0, 0, (char*)ts_pck, 188);
				if (e) 
					fprintf(stdout, "Error %s sending RTP packet\n", gf_error_to_string(e));
				if (status>=GF_M2TS_STATE_PADDING) break;
			}
			break;
		}
		if (real_time) {
			/*abort*/
			if (gf_prompt_has_input()) {
				char c = gf_prompt_get_char();
				if (c == 'q') break;
			}
			fprintf(stdout, "M2TS: time %d - TS time %d - avg bitrate %d\r", gf_m2ts_get_sys_clock(muxer), gf_m2ts_get_ts_clock(muxer), muxer->avg_br);
		} else if (run_time) {
			if (gf_m2ts_get_ts_clock(muxer) > run_time) {
				fprintf(stdout, "Stoping multiplex at %d ms (requested runtime %d ms)\n", gf_m2ts_get_ts_clock(muxer), run_time);
				break;
			}
		} else if (status==GF_M2TS_STATE_EOS) {
			break;
		}
	}


exit:
	if (ts_file) fclose(ts_file);
	if (ts_udp) gf_sk_del(ts_udp);
	if (ts_rtp) gf_rtp_del(ts_rtp);
	if (ts_out) free(ts_out);
	m2ts_mux_del(muxer);
	
	for (i=0; i<nb_progs; i++) {
		for (j=0; j<progs[i].nb_streams; j++) {
			progs[i].streams[j].input_ctrl(&progs[i].streams[j], GF_ESI_INPUT_DESTROY, NULL);
		}
		if (progs[i].iod) gf_odf_desc_del((GF_Descriptor*)progs[i].iod);
		if (progs[i].mp4) gf_isom_close(progs[i].mp4);
	}
	gf_sys_close();
	return 1;
}

