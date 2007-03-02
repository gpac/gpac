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
#include "mp42ts.h"

static u32 track_number, sample_number, sample_count; 
static GF_ISOFile *mp4; /* berk!!!*/

void usage() 
{
	fprintf(stderr, "usage: mp42ts input.mp4 output.ts\n");
}

/************************************
 * Section-related functions 
 ************************************/
void m2ts_mux_table_update(M2TS_Mux_Stream *stream, u8 table_id, u16 table_id_extension,
						   u8 *table_payload, u32 table_payload_length, 
						   Bool use_syntax_indicator, Bool private_indicator,
						   Bool use_checksum) 
{
	u32 count, i;
	u32 overhead_size;
	u32 offset;
	u32 section_number, nb_sections;
	M2TS_Mux_Table *table;
	u32 maxSectionLength;
	M2TS_Mux_Section *section;
	GF_BitStream *bs;

	table = NULL;
	count = gf_list_count(stream->tables);
	for (i=0; i<count; i++) {
		table = gf_list_get(stream->tables, i);
		if (table->table_id == table_id) {
			u32 j, count2;
			count2 = gf_list_count(table->sections);
			for (j=0; j<count2; j++) {
				M2TS_Mux_Section *section = gf_list_get(table->sections, j);
				free(section->data);
				free(section);
			}
			gf_list_reset(table->sections);
	
			table->version_number = (table->version_number + 1)%0x1F;
			break;
		}
		table = NULL;
	}

	if (!table) {
		GF_SAFEALLOC(table, M2TS_Mux_Table);
		table->table_id = table_id;
		table->sections = gf_list_new();	
		gf_list_add(stream->tables, table);
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
	
	offset = 0;
	while (offset < table_payload_length) {
		u32 remain;
		section = malloc(sizeof(M2TS_Mux_Section));

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
	
		gf_list_add(table->sections, section);
	}
}

/* length of adaptation_field_length; */ 
#define ADAPTATION_LENGTH_LENGTH 1
/* discontinuty flag, random access flag ... */
#define ADAPTATION_FLAGS_LENGTH 1 
/* length of encoded pcr */
#define PCR_LENGTH 6

u32 add_adaptation(GF_BitStream *bs, 
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
	
	table = gf_list_get(stream->tables, stream->current_table);
	assert(table);

	section = gf_list_get(table->sections, stream->current_section);
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
		add_adaptation(bs, 0, 0, 0, padding_length);

	if (!stream->current_section_offset) { //write pointer field
		gf_bs_write_u8(bs, 0); /* no concatenations of sections in ts packets, so start address is 0 */
	}

	gf_bs_write_data(bs, section->data + stream->current_section_offset, payload_length);
	stream->current_section_offset += payload_length;
	
	if (stream->current_section_offset == section->length) {
		stream->current_section_offset = 0;
		stream->current_section++;
		if (stream->current_section == gf_list_count(table->sections)) {
			stream->current_section = 0;
			stream->current_table++;
			if (stream->current_table == gf_list_count(stream->tables)) {
				stream->current_table = 0;
				/* TODO: store the time for the next section to be sent */
				stream->time++;
			}
		}
	}

}



s32 m2ts_stream_process_pat(M2TS_Mux *muxer, M2TS_Mux_Stream *stream)
{
	if (stream->table_needs_update) { /* generate table payload */
		u32 i;
		u8 *payload;
		u32 nb_programs;

		nb_programs = gf_list_count(muxer->programs);
		payload = malloc(nb_programs * 4);

		for (i=0; i < nb_programs ;i++) {
			M2TS_Mux_Program *program = gf_list_get(muxer->programs, i);
			//program number
			payload[4*i] = program->number >> 8;
			payload[4*i+1] = program->number;
			// program pmt pid < 2^13-1
			payload[4*i+2] = program->pmt->pid >> 8 | 0xe0;
			payload[4*i+3] = program->pmt->pid ;
		}
		m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PAT, muxer->ts_id, payload, nb_programs*4, 1, 0, 0);
		stream->table_needs_update = 0;
	}
	/* test process time */
	return stream->time;
}

s32 m2ts_stream_process_pmt(M2TS_Mux *muxer, M2TS_Mux_Stream *stream)
{
	if (stream->table_needs_update) { /* generate table payload */
		u32 i, count;
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
		count = gf_list_count(stream->program->streams);
		for (i =0; i <count; i++) {
			M2TS_Mux_Stream *es;
			es = gf_list_get(stream->program->streams, i);
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
							
		}
	
		gf_bs_get_content(bs, &payload, &length);
		gf_bs_del(bs);

		m2ts_mux_table_update(stream, GF_M2TS_TABLE_ID_PMT, stream->program->number, payload, length, 1, 0, 0);
		stream->table_needs_update = 0;
	}
	return stream->time;
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

s32 m2ts_stream_process_stream(M2TS_Mux *muxer, M2TS_Mux_Stream *stream)
{
	u32 tmp;
	GF_ISOSample *sample;
	GF_BitStream *bs;
	Bool PUSI;
	u32 remain;
	Bool is_rap;
	u32 adaptation_length;
	u32 payload_length;
	u32 padding_length;
	u8 *ts_data;
	u32 ts_data_len;

	sample = gf_isom_get_sample(mp4, track_number, sample_number, &tmp);
	stream->au = malloc(sample->dataLength);
	memcpy(stream->au, sample->data, sample->dataLength);
	stream->au_size = sample->dataLength;

	bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);

	if (!stream->au_offset){
		PUSI = 1;
	} else {
		PUSI = 0;
	}

	gf_bs_write_int(bs,	0x47, 8); // sync byte
	gf_bs_write_int(bs,	0, 1);    // error indicator
	gf_bs_write_int(bs,	PUSI, 1); // start ind
	gf_bs_write_int(bs,	0, 1);	  // transport priority
	gf_bs_write_int(bs,	stream->pid, 13); // pid
	gf_bs_write_int(bs,	0, 2);    // scrambling

	remain = stream->au_size + PES_HEADER_LENGTH*PUSI - stream->au_offset;

	if (stream->mpeg2_stream_type == GF_M2TS_VIDEO_MPEG2 && sample->IsRAP) is_rap=1;
	else is_rap = 0;

	if (PUSI) {
		adaptation_length = add_adaptation(bs, (stream->program->pcr == stream), sample->DTS, is_rap,
										   padding_length);
	}

	payload_length = 184 - adaptation_length;

	if (PUSI){
		u32 pes_payload_length;
		
		m2ts_stream_add_pes_header(bs, stream, 
									1, sample->DTS, 
									1, sample->CTS_Offset + sample->DTS,
									sample->dataLength);
		pes_payload_length = payload_length - PES_HEADER_LENGTH;
		gf_bs_write_data(bs, stream->au, pes_payload_length);
		stream->au_offset += pes_payload_length;
//		fprintf(stdout, "TS payload length %d - total %d\n", pes_payload_length, stream->nb_bytes_written);
	} else {
		gf_bs_write_data(bs, stream->au+stream->au_offset, payload_length);
		stream->au_offset += payload_length;
//		fprintf(stdout, "TS payload length %d - total %d \n", payload_length, stream->nb_bytes_written);
	}
	gf_bs_get_content(bs, &ts_data, &ts_data_len);
	gf_bs_del(bs);
	gf_isom_sample_del(&sample);

	return 2;
}


M2TS_Mux_Stream *m2ts_stream_new(u32 pid) {
	M2TS_Mux_Stream *stream;

	GF_SAFEALLOC(stream, M2TS_Mux_Stream);
	stream->tables = gf_list_new();
	stream->pid = pid;
	stream->process = m2ts_stream_process_stream;

	return stream;
}

M2TS_Mux_Stream *m2ts_program_stream_add(M2TS_Mux_Program *program, u32 pid, Bool is_pcr)
{
	M2TS_Mux_Stream *stream;

	stream = m2ts_stream_new(pid);
	stream->program = program;
	if (is_pcr) program->pcr = stream; 
	gf_list_add(program->streams, stream);
	if (program->pmt) program->pmt->table_needs_update = 1;

	return stream;
}

M2TS_Mux_Program *m2ts_mux_program_add(M2TS_Mux *muxer, u32 program_number, u32 pmt_pid)
{
	M2TS_Mux_Program *program;

	GF_SAFEALLOC(program, M2TS_Mux_Program);
	program->mux = muxer;
	program->streams = gf_list_new();
	program->number = program_number;
	gf_list_add(muxer->programs, program);
	program->pmt = m2ts_stream_new(pmt_pid);
	program->pmt->program = program;
	program->pmt->process = m2ts_stream_process_pmt;
	muxer->pat->table_needs_update = 1;
	return program;
}

M2TS_Mux *m2ts_mux_new()
{
	M2TS_Mux *muxer;
	GF_SAFEALLOC(muxer, M2TS_Mux);
	muxer->programs = gf_list_new();
	muxer->pat = m2ts_stream_new(0); /* 0 = PAT_PID */
	muxer->pat->process = m2ts_stream_process_pat;
	return muxer;
}


void m2ts_mux_process(M2TS_Mux *muxer)
{
	M2TS_Mux_Program *program;
	M2TS_Mux_Stream *stream, *stream_to_process;
	u32 i, j, count1, count2;
	u8 packet[188];
	s32 delta, delta_tmp;

	stream_to_process = NULL;

	delta = muxer->pat->process(muxer, muxer->pat);
	stream_to_process = muxer->pat;

	count1 = gf_list_count(muxer->programs);
	for (i = 0; i<count1; i++) {
		program = gf_list_get(muxer->programs, i);
		delta_tmp = program->pmt->process(muxer, program->pmt);
		if (delta_tmp < delta) {
			delta = delta_tmp;
			stream_to_process = program->pmt;
		}

		count2 = gf_list_count(program->streams);
		for (j=0; j<count2; j++) {
			stream = gf_list_get(program->streams, j);
			delta_tmp = stream->process(muxer, stream);
			if (delta_tmp < delta) {
				delta = delta_tmp;
				stream_to_process = stream;
			}
		}
	}

	if (!stream_to_process) {
		/* padding packets ?? */
	} else {
		m2ts_mux_table_get_next_packet(stream_to_process, packet);
		fwrite(packet, 1, 188, muxer->ts_out); 
		muxer->time++;
	}
}

void main(int argc, char **argv)
{
	u32 i;
	M2TS_Mux *muxer;
	M2TS_Mux_Program *program;
	M2TS_Mux_Stream *stream;

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
	
	program = m2ts_mux_program_add(muxer, 10, 100);
	stream = m2ts_program_stream_add(program, 100, 1);
	stream->mpeg2_stream_type = GF_M2TS_VIDEO_MPEG2;

	/* Open ISO file  */
	if (gf_isom_probe_file(argv[1])) 
		mp4 = gf_isom_open(argv[1], GF_ISOM_OPEN_READ, 0);
	else {
		fprintf(stderr, "Error opening %s - not an ISO media file\n", argv[1]);
		return;
	}

	track_number = 1;
	sample_count = gf_isom_get_sample_count(mp4, track_number);
	sample_number = 0; 

	i= 0;
	while (i<4) {
		m2ts_mux_process(muxer);
		i++;
	}


#if 0


	muxer.TS_Rate      = 512000;	// bps
	muxer.PAT_interval = 0.500;	// s
	muxer.PMT_interval = 0.500;	// s
	muxer.SI_interval  = 1;		// s

	/* nb TS packet before sending new PAT */
	muxer.send_pat = (u32)(muxer.TS_Rate * muxer.PAT_interval / 188);  
	/* nb TS packet before sending new PMT */
	muxer.send_pmt = (u32)(muxer.TS_Rate * muxer.PMT_interval / 188);

	muxer.use_sl = 0;
	muxer.log_level = LOG_PES;
	initialize_muxer(&muxer);
	
	CreateTSPacketsForPAT(&muxer);
	muxer.on_event(&muxer, GF_M2TS_EVT_PAT, NULL);
	fflush(muxer.ts_out);

	CreateTSPacketsForPMTs(&muxer);
	muxer.on_event(&muxer, GF_M2TS_EVT_PMT, NULL);
	fflush(muxer.ts_out);

/*
	CreateTSPacketsForSDT(muxer, "GPAC", "GPAC Service");
	muxer.on_event(muxer, GF_M2TS_EVT_SDT, NULL);
*/

	while (!muxer.end) {
		/*if (muxer.insert_SI == muxer.send_pat){
			muxer.on_event(&muxer, GF_M2TS_EVT_PAT, NULL);
			// muxer.on_event(&muxer, GF_M2TS_EVT_SDT, NULL); 
		} else if (muxer.insert_SI == muxer.send_pmt){
			muxer.on_event(&muxer, GF_M2TS_EVT_PMT, NULL);
		} */
		muxer.on_event(&muxer, GF_M2TS_EVT_ES, NULL);		
	}
	
	gf_isom_close(muxer.mp4_in);
#endif

	if (muxer->ts_out) fclose(muxer->ts_out);
}

