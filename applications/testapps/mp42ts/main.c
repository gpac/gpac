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
#include "mp42ts.h"

void usage() 
{
	fprintf(stderr, "usage: mp42ts input.mp4 output.ts\n");
}

static GFINLINE void m2ts_dump_time(M2TS_Time *time, char *name)
{
	fprintf(stdout, "%s: %d%03d\n", name, time->sec, time->nanosec/1000000);
}

#define m2ts_time_less(_a, _b) ( ((_a.sec<_b.sec) || (_a.nanosec<_b.nanosec)) ? 1 : 0 )
#define m2ts_time_less_or_equal(_a, _b) ( ((_a.sec<_b.sec) || ((_a.sec==_b.sec) && (_a.nanosec<=_b.nanosec))) ? 1 : 0 )

static GFINLINE void m2ts_time_inc(M2TS_Time *time, u32 delta_inc_num, u32 delta_inc_den)
{
	u64 n_sec;
	u32 sec = delta_inc_num / delta_inc_den;

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

	prev_table = NULL;
	table = stream->tables;
	while (table) {
		if (table->table_id == table_id) {
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

		gf_bs_get_content(bs, &section->data, &section->length); 
		gf_bs_del(bs);

		if (use_syntax_indicator) {
			u32 CRC;
			CRC = gf_m2ts_crc32(section->data,section->length-CRC_LENGTH); 
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
	stream->table_needs_update = 1;
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
			padding_length = payload_length - section->length - stream->current_section_offset; 
			payload_length -= padding_length;
		}
	}
	assert(payload_length + stream->current_section_offset <= section->length);
	
	gf_bs_write_int(bs,	0, 1);    // priority indicator
	gf_bs_write_int(bs,	stream->pid, 13); // section pid
	gf_bs_write_int(bs,	0, 2);    // scrambling indicator
	gf_bs_write_int(bs,	adaptation_field_control, 2);    // we do not use adaptation field for sections 
	gf_bs_write_int(bs,	stream->continuity_counter, 4);   // continuity counter
	if (stream->continuity_counter < 15) stream->continuity_counter++;
	else stream->continuity_counter=0;

	if (adaptation_field_control != M2TS_ADAPTATION_NONE)
		m2ts_add_adaptation(bs, 0, 0, 0, padding_length);

	if (!stream->current_section_offset) { //write pointer field
		gf_bs_write_u8(bs, 0); /* no concatenations of sections in ts packets, so start address is 0 */
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
			if (!stream->current_table) {
				stream->current_table = stream->tables;
				/*update time*/
				m2ts_time_inc(&stream->time, stream->refresh_rate_ms, 1000);
			}
			stream->current_section = stream->current_table->section;
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
		gf_bs_get_content(bs, &payload, &size);
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
	
		if (1 /* !iod */) {
			gf_bs_write_int(bs,	0, 12); // program info length =0
		} else {
#if 0
			u32 len;
			len = iod->length + 4;
			gf_bs_write_int(bs,	len, 12); // program info length = iod len
			/*for (i =0; i< gf_list_count(program->desc); i++)
			{
				// program descriptors
				// IOD
			}*/
			gf_bs_write_int(bs,	29, 8); // tag
			len = iod->length + 2;
			gf_bs_write_int(bs,	len, 8); // length
			gf_bs_write_int(bs,	2, 8);  // Scope_of_IOD_label : 0x10 iod unique a l'intérieur de programme
													// 0x11 iod unoque dans la flux ts
			gf_bs_write_int(bs,	2, 8);  // IOD_label

			gf_bs_write_data(bs, iod->data, iod->length);  // IOD
#endif
		}	
		es = stream->program->streams;
		while (es) {
			gf_bs_write_int(bs,	es->mpeg2_stream_type, 8);
			gf_bs_write_int(bs,	0x7, 3); // reserved
			gf_bs_write_int(bs,	es->pid, 13);
			gf_bs_write_int(bs,	0xF, 4); // reserved
			
			/* Second Loop Descriptor */
			if (0 /*iod*/) {
#if 0
				gf_bs_write_int(bs,	4, 12); // ES info length = 4 :only SL Descriptor
				gf_bs_write_int(bs,	30, 8); // tag
				gf_bs_write_int(bs,	1, 8); // length
				gf_bs_write_int(bs,	es->mpeg4_es_id, 16);  // mpeg4_esid
#endif
			} else {
				gf_bs_write_int(bs,	0, 12);
			}
			es = es->next;
		}
	
		gf_bs_get_content(bs, &payload, &length);
		gf_bs_del(bs);

		m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PMT, stream->program->number, payload, length, 1, 0, 0);
		stream->table_needs_update = 0;
		free(payload);
	}
	return 1;
}

#define PES_HEADER_LENGTH 19
u32 m2ts_stream_add_pes_header(GF_BitStream *bs, M2TS_Mux_Stream *stream, 
								Bool use_dts, u64 dts, 
								Bool use_pts, u64 cts, 
								u32 au_length)
{
	u32 pes_len;
	u32 pes_header_data_length;

	pes_header_data_length = 10;

	gf_bs_write_int(bs,	0x1, 24);//packet start code
	gf_bs_write_u8(bs,	stream->mpeg2_stream_id);// stream id 

	pes_len = au_length + 13; // 13 = header size (including PTS/DTS)
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

	gf_bs_write_int(bs, pes_header_data_length, 8);
	
	if (use_pts && use_dts){
		u64 t;
		
		gf_bs_write_int(bs, 0x2, 4); // reserved '0010'
		t = ((cts >> 30) & 0x7);
		gf_bs_write_long_int(bs, t, 3);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = ((cts >> 15) & 0x7fff);
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit
		t = cts & 0x7fff;
		gf_bs_write_long_int(bs, t, 15);
		gf_bs_write_int(bs, 1, 1); // marker bit

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

	return pes_len+4; // 4 = start code + stream_id
}

Bool m2ts_stream_process_stream(M2TS_Mux *muxer, M2TS_Mux_Stream *stream)
{
	if (stream->pck_offset == stream->pck.data_len) {
		if (stream->ifce->caps & GF_ESI_AU_PULL_CAP) {
			if (stream->pck_offset) stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_RELEASE, NULL);

			/*EOF*/
			if (stream->ifce->caps & GF_ESI_STERAM_IS_OVER) return 0;
			stream->ifce->input_ctrl(stream->ifce, GF_ESI_INPUT_DATA_PULL, &stream->pck);
		}
		stream->pck_offset = 0;

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
	}
	return 1;
}

void m2ts_mux_pes_get_next_packet(M2TS_Mux_Stream *stream, u8 *packet)
{
	GF_BitStream *bs;
	Bool au_start, is_rap, needs_pcr;
	u32 remain, adaptation_field_control, payload_length, padding_length;

	assert(stream->pid);
	bs = gf_bs_new(packet, 188, GF_BITSTREAM_WRITE);

	au_start = 0;
	if (!stream->pck_offset && (stream->pck.flags & GF_ESI_DATA_AU_START) ) au_start = 1;
	remain = stream->pck.data_len - stream->pck_offset;

	adaptation_field_control = M2TS_ADAPTATION_NONE;
	payload_length = 184 - PES_HEADER_LENGTH*au_start;
	needs_pcr = (au_start && (stream==stream->program->pcr) ) ? 1 : 0;

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
	gf_bs_write_int(bs,	au_start, 1); // start ind
	gf_bs_write_int(bs,	0, 1);	  // transport priority
	gf_bs_write_int(bs,	stream->pid, 13); // pid
	gf_bs_write_int(bs,	0, 2);    // scrambling
	gf_bs_write_int(bs,	adaptation_field_control, 2);    // we do not use adaptation field for sections 
	gf_bs_write_int(bs,	stream->continuity_counter, 4);   // continuity counter
	if (stream->continuity_counter < 15) stream->continuity_counter++;
	else stream->continuity_counter=0;

	if (au_start && (stream->pck.flags & GF_ESI_DATA_AU_RAP) ) is_rap = 1;
	else is_rap = 0;

	if (adaptation_field_control != M2TS_ADAPTATION_NONE) {
		/*FIXME - WE NEED A REAL PCR, NOT THE DTS*/
		m2ts_add_adaptation(bs, needs_pcr, stream->pck.dts, is_rap, padding_length);
	}

	/*FIXME - we need proper packetization here in case we're not fed with full AUs*/
	if (au_start) {
		m2ts_stream_add_pes_header(bs, stream, 1, stream->pck.dts, 1, stream->pck.cts, stream->pck.data_len);
	}

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
		default:
			break;
		}
		/*just pick first valid stream_id in visual range*/
		stream->mpeg2_stream_id = 0xE0;
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
	}
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

M2TS_Mux *m2ts_mux_new()
{
	GF_BitStream *bs;
	M2TS_Mux *muxer;
	GF_SAFEALLOC(muxer, M2TS_Mux);
	muxer->pat = m2ts_stream_new(0); /* 0 = PAT_PID */
	muxer->pat->process = m2ts_stream_process_pat;
	muxer->pat->refresh_rate_ms = M2TS_PSI_REFRESH_RATE;

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

void m2ts_mux_update_config(M2TS_Mux *mux)
{
	M2TS_Mux_Program *prog;

	mux->bit_rate = 0;

	/*get PAT bitrate*/
	m2ts_mux_table_update_bitrate(mux, mux->pat);
	mux->bit_rate += mux->pat->bit_rate;

	prog = mux->programs;
	while (prog) {
		M2TS_Mux_Stream *stream = prog->streams;
		while (stream) {
			/*!! WATCHOUT - this is raw bitrate without PES header overhead !!*/
			mux->bit_rate += stream->bit_rate;
			/*update PCR every 100ms - we need at least 8 bytes without padding*/
			if (stream == prog->pcr) mux->bit_rate += 8*8*10;

			/*reset mux time*/
			stream->time.sec = stream->time.nanosec = 0;
			stream = stream->next;
		}
		/*get PMT bitrate*/
		m2ts_mux_table_update_bitrate(mux, prog->pmt);
		mux->bit_rate += prog->pmt->bit_rate;
		prog = prog->next;
	}
	/*reset mux time*/
	mux->time.sec = mux->time.nanosec = 0;
}


Bool m2ts_mux_process(M2TS_Mux *muxer)
{
	M2TS_Mux_Program *program;
	M2TS_Mux_Stream *stream, *stream_to_process;
	u8 packet[188];
	M2TS_Time time;
	Bool res, is_over;

	stream_to_process = NULL;
	time = muxer->time;
	
	is_over = 1;

	res = muxer->pat->process(muxer, muxer->pat);
	if (res && m2ts_time_less_or_equal(muxer->pat->time, time) ) {
		time = muxer->pat->time;
		stream_to_process = muxer->pat;
	}

	program = muxer->programs;
	while (program) {
		res = program->pmt->process(muxer, program->pmt);
		if (res && m2ts_time_less(program->pmt->time, time) ) {
			time = program->pmt->time;
			stream_to_process = program->pmt;
		}
		stream = program->streams;
		while (stream) {
			res = stream->process(muxer, stream);
			if (res) {
				is_over = 0;
				if (m2ts_time_less(stream->time, time)) {
					time = stream->time;
					stream_to_process = stream;
				}
			}
			stream = stream->next;
		}
		program = program->next;
	}

	if (!stream_to_process) {
		/* padding packets ?? */
		fwrite(muxer->null_pck, 1, 188, muxer->ts_out); 
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Inserting empty packet at %d:%d\n", time.sec, time.nanosec));
	} else {
		if (stream_to_process->tables) {
			m2ts_mux_table_get_next_packet(stream_to_process, packet);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sending table packet from PID %d at %d:%d\n", stream_to_process->pid, time.sec, time.nanosec));
//			m2ts_dump_time(&stream_to_process->time, "adding PSI tables");
		} else {
			m2ts_mux_pes_get_next_packet(stream_to_process, packet);
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[MPEG2-TS Muxer] Sending PES packet from PID %d at %d:%d\n", stream_to_process->pid, time.sec, time.nanosec));
		}
		fwrite(packet, 1, 188, muxer->ts_out); 
	}
	m2ts_time_inc(&muxer->time, 1504/*188*8*/, muxer->bit_rate);
	return is_over;
}

typedef struct
{
	GF_ISOFile *mp4;
	u32 track, sample_number, sample_count;
	GF_ISOSample *sample;
} GF_ESIMP4;

static GF_Err mp4_input_ctrl(GF_ESInterface *ifce, u32 act_type, void *param)
{
	GF_ESIMP4 *priv = (GF_ESIMP4 *)ifce->input_udta;
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
		pck->dts = priv->sample->DTS;
		pck->cts = priv->sample->DTS + priv->sample->CTS_Offset;
		pck->data_len = priv->sample->dataLength;
		pck->data = priv->sample->data;
		pck->flags = GF_ESI_DATA_AU_START | GF_ESI_DATA_AU_END;
		if (priv->sample->IsRAP) pck->flags |= GF_ESI_DATA_AU_RAP;
	}
		return GF_OK;
	case GF_ESI_INPUT_DATA_RELEASE:
		if (priv->sample) {
			gf_isom_sample_del(&priv->sample);
			priv->sample_number++;
			if (priv->sample_number==priv->sample_count) 
				ifce->caps |= GF_ESI_STERAM_IS_OVER;
		}
		return GF_OK;
	default:
		return GF_BAD_PARAM;
	}
}

static void fill_es_ifce(GF_ESInterface *ifce, GF_ISOFile *mp4, u32 track_num)
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
	ifce->bit_rate = (u32) avg_rate;
	ifce->duration = (Double) (s64) gf_isom_get_media_duration(mp4, track_num);
	ifce->duration /= ifce->timescale;

	ifce->input_ctrl = mp4_input_ctrl;
	ifce->input_udta = priv;
}

void main(int argc, char **argv)
{
	u32 i;
	M2TS_Mux *muxer;
	M2TS_Mux_Program *program;
	M2TS_Mux_Stream *stream;
	static u32 track_number, sample_number, sample_count; 
	static GF_ISOFile *mp4; /* berk!!!*/

	GF_ESInterface st1, st2;

	if (argc < 2) {
		usage();
		return;
	}

	gf_sys_init();

	muxer = m2ts_mux_new();
	
	/* Open mpeg2ts file*/
	muxer->ts_out = fopen(argv[2], "wb");
	if (!muxer->ts_out) {
		fprintf(stderr, "Error opening %s\n", argv[2]);
		return;
	}
	
	/* Open ISO file  */
	if (gf_isom_probe_file(argv[1])) 
		mp4 = gf_isom_open(argv[1], GF_ISOM_OPEN_READ, 0);
	else {
		fprintf(stderr, "Error opening %s - not an ISO media file\n", argv[1]);
		return;
	}

	fill_es_ifce(&st1, mp4, 1);
	fill_es_ifce(&st2, mp4, 2);

	program = m2ts_mux_program_add(muxer, 10, 100);
	m2ts_program_stream_add(program, &st1, 110, 1);
	m2ts_program_stream_add(program, &st2, 111, 1);

	m2ts_mux_update_config(muxer);

	while (!m2ts_mux_process(muxer)) {}

	if (muxer->ts_out) fclose(muxer->ts_out);
	m2ts_mux_del(muxer);
	gf_isom_close(mp4);
	
	free(st1.input_udta);
	free(st2.input_udta);

	gf_sys_close();
}

