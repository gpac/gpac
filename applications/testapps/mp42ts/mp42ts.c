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
#include <gpac/isomedia.h>
#include <gpac/bifs.h>
#include <gpac/mpegts.h>
#include <gpac/avparse.h>
#include "mp42ts.h"

/************************************
 * Section-related functions 
 ************************************/
GF_List *CreateSections(MP42TS_Buffer *B, u8 table_id, u16 sec_id, u32 version_number) {
	u32 maxSectionLength;
	u32 remain;
	GF_List *sections;
	GF_M2TS_SectionFilter *section;
	u32 first_header_length, second_header_length, crc_length;

	first_header_length  = 3; /* header till the last bit of the section_length field */
	second_header_length = 5; /* header from the last bit of the section_length field to the payload */
	crc_length = 4; 

	switch (table_id) {
	case GF_M2TS_TABLE_ID_PMT:
	case GF_M2TS_TABLE_ID_PAT:
	case GF_M2TS_TABLE_ID_SDT_ACTUAL:
	case GF_M2TS_TABLE_ID_SDT_OTHER:
	case GF_M2TS_TABLE_ID_BAT:
		maxSectionLength = 1021;
		break;
	case GF_M2TS_TABLE_ID_MPEG4_BIFS:
	case GF_M2TS_TABLE_ID_MPEG4_OD:
		maxSectionLength = 4093;
		break;
	default:
		fprintf(stderr, "Cannot create sections for table with id %d\n", table_id);
		return NULL;
	}

	sections = gf_list_new();	
	remain = B->length;
	while (remain > 0) {
		GF_SAFEALLOC(section, GF_M2TS_SectionFilter);

		section->section = B->data + B->length - remain; 
		
		if (second_header_length + remain + crc_length > maxSectionLength) {
			section->length = maxSectionLength;
			remain -= maxSectionLength;
		} else {
			section->length = second_header_length + remain + crc_length;
			remain = 0;
		}
		section->table_id = table_id;
		section->syntax_indicator = 1; // égale à 1, sauf pour les section privé en version court = 0.
		section->sec_id = sec_id;
		section->version_number = 1;
		section->current_next_indicator = 1;
		if (table_id == GF_M2TS_TABLE_ID_PMT) {
			section->section_number = 0;
			section->last_section_number = 0;
		} else {
			section->last_section_number = B->length / maxSectionLength; /* wrong: omits CRC & second header!! */
		}
		gf_list_add(sections, section);
	}
	return sections;	
}

MP42TS_Buffer *EncodeSections(GF_List *sections)
{
	MP42TS_Buffer *B;
	GF_M2TS_SectionFilter *section;
	u32 CRC, len;
	GF_BitStream *bs;
	u32 first_header_length, second_header_length, crc_length;

	first_header_length = 3; /* header till the last bit of the section_length field */
	second_header_length = 5; /* header from the last bit of the section_length field to the payload */
	crc_length = 4; 

	section = gf_list_get(sections, 0); 

	GF_SAFEALLOC(B, MP42TS_Buffer);

	bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);

	/* first header (not included in section length */
	gf_bs_write_int(bs,	section->table_id, 8);
	gf_bs_write_int(bs,	section->syntax_indicator, 1);
	gf_bs_write_int(bs,	0, 1); // reserved
	gf_bs_write_int(bs,	1, 2); // reserved
	gf_bs_write_int(bs,	section->length, 12);
	
	/* second header */
	gf_bs_write_int(bs,	section->sec_id, 16);
	gf_bs_write_int(bs,	1, 2); // reserved
	gf_bs_write_int(bs,	section->version_number, 5);
	gf_bs_write_int(bs,	section->current_next_indicator, 1);
	gf_bs_write_int(bs,	section->section_number, 8);
	if (section->table_id != GF_M2TS_TABLE_ID_PMT) section->section_number++;
	gf_bs_write_int(bs,	section->last_section_number, 8);

	/* real payload */
	len = section->length - second_header_length - crc_length;  
	gf_bs_write_data(bs, section->section, len);
	
	/* place holder for CRC */
	gf_bs_write_u32(bs, 0);

	gf_bs_get_content(bs, &B->data, &B->length); 
	gf_bs_del(bs);

	CRC = gf_m2ts_crc32(B->data,B->length-4); 
	B->data[B->length-4] = (CRC >> 24) & 0xFF;
	B->data[B->length-3] = (CRC >> 16) & 0xFF;
	B->data[B->length-2] = (CRC >> 8) & 0xFF;
	B->data[B->length-1] = CRC & 0xFF;

	return B;
}

GF_List *CreateTSPacketsFromSections(MP42TS_Buffer *sec_B, u32 pid, u32 *continuity_counter)
{
	u32 remain;
	u32 PUSI, cc;
	MP42TS_Buffer *B;
	GF_BitStream *bs;
	GF_List *ts_packet;

	ts_packet = gf_list_new();

	PUSI = 1;
	if (!continuity_counter){ 
		cc = 0;
	}else{
		cc = *continuity_counter;
	}
	
	remain = sec_B->length;
	while (remain > 0) {
		bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);
		gf_bs_write_int(bs,	0x47, 8); // sync
		gf_bs_write_int(bs,	0, 1); // error indicator
		gf_bs_write_int(bs,	PUSI, 1); // payload start indicator
		
		gf_bs_write_int(bs,	0, 1); // priority indicator
		gf_bs_write_int(bs,	pid, 13); // section pid
		gf_bs_write_int(bs,	0, 2); // scrambling indicator
		gf_bs_write_int(bs,	1, 2); // pas d'adaptation pour les section (NB:on peut avoir un champ d'apaptation dans les section).
		gf_bs_write_int(bs,	cc, 4); // continuity counter
		if (cc < 15) cc +=1;
		else cc =0;
		
		// pointer field: je commence la section au debut de payload de paquet ts.
		// dans la cas de section successif de même PID=> pointer field peut etre !=0.
		if (PUSI == 1) gf_bs_write_int(bs,	0, 8); 							

		if (remain > (184 - PUSI)) {
			gf_bs_write_data(bs, sec_B->data + sec_B->length - remain, 183);
			remain -= (184 - PUSI);
		} else {
			u32 padding = (184 - PUSI) - remain;
			gf_bs_write_data(bs, sec_B->data + sec_B->length - remain, remain);
			remain = 0;
			while (padding) {
				gf_bs_write_int(bs,	0xff, 8);
				padding--;
			}
		} 

		PUSI = 0;
		
		GF_SAFEALLOC(B, MP42TS_Buffer);
		gf_bs_get_content(bs, &B->data, &B->length);
		gf_bs_del(bs);
		
		if (continuity_counter) *continuity_counter = cc;
		
		gf_list_add(ts_packet, B->data);
	}

	return ts_packet;
}

/************************************************************
 * SL-Packetized Stream related functions 
 ************************************************************/
void config_sample_hdr(M2TS_mux_stream *stream)
{
	GF_SAFEALLOC(stream->SLHeader, GF_SLHeader);
	if (stream->sample){
		stream->SLHeader->accessUnitStartFlag = 1;
		stream->SLHeader->accessUnitEndFlag = 1;
		stream->SLHeader->accessUnitLength = stream->sample->dataLength;
		stream->SLHeader->randomAccessPointFlag = stream->sample->IsRAP;
		stream->SLHeader->compositionTimeStampFlag = 1;
		stream->SLHeader->compositionTimeStamp = (stream->sample->CTS_Offset+stream->sample->DTS)*90000/1000;
		stream->SLHeader->decodingTimeStampFlag = 1;
		stream->SLHeader->decodingTimeStamp = stream->sample->DTS*90000/1000;
		printf("SL Header: DTS "LLD", CTS "LLD"\n", stream->SLHeader->decodingTimeStamp, stream->SLHeader->compositionTimeStamp);
	}
}

void SLpacket_in_pes(M2TS_mux_stream *stream, char *Packet, u32 Size)
{
	config_sample_hdr(stream);
	gf_sl_packetize(stream->SLConfig, stream->SLHeader, stream->sample->data, stream->sample->dataLength, &stream->sl_packet, &stream->sl_packet_len);
}

GF_List *CreateTSPacketsFromSLPacket(M2TS_mux_stream *stream, char *SLPacket, u32 SL_Size)
{
	u8 table_id;
	u16 table_id_extension;
	MP42TS_Buffer *B;
	GF_List *sections, *ts_packet;
		
	if (stream->MP4_type == GF_ISOM_MEDIA_SCENE){
		table_id = GF_M2TS_TABLE_ID_MPEG4_BIFS;
	}else if (stream->MP4_type == GF_ISOM_MEDIA_OD){
		table_id = GF_M2TS_TABLE_ID_MPEG4_OD;
	}
	table_id_extension = 0;

	GF_SAFEALLOC(B, MP42TS_Buffer);
	B->length = SL_Size;
	B->data = SLPacket;
	sections = CreateSections(B, table_id, table_id_extension, stream->SL_section_version_number);	
	B = EncodeSections(sections);
	ts_packet = CreateTSPacketsFromSections(B, stream->mpeg2_es_pid, &stream->continuity_counter);	

	free(B);
	stream->SL_section_version_number ++;
	if (stream->SL_section_version_number > 32) stream->SL_section_version_number = 0;

	return ts_packet;
}

MP42TS_Buffer *get_iod(GF_BitStream *bs)
{
	GF_Err e;
	GF_InitialObjectDescriptor *iod;
	u32 DescSize =0;
	GF_BitStream *bs_out;
	MP42TS_Buffer *B;
	iod = (GF_InitialObjectDescriptor *)gf_odf_new_iod();
		
	e = gf_odf_read_iod(bs, iod, DescSize);
	
	bs_out = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);
	e = gf_odf_write_iod(bs_out, iod);

	GF_SAFEALLOC(B, MP42TS_Buffer);
	B->data = malloc(DescSize);
	gf_bs_get_content(bs_out, &B->data, &B->length); 
	gf_bs_del(bs);
	
	return B;
}

/**********************************************************************************
 * PSI related functions
 **********************************************************************************/

/*************************** PMT *****************************/
void InitializePMT(GF_M2TS_Program *program)
{
	u32 i;
	
	program->pcr_pid = 0;
	for (i = 0; i<gf_list_count(program->streams); i++) {
		M2TS_mux_stream *stream = (M2TS_mux_stream *)gf_list_get(program->streams, i);
		if (stream->MP2_type == GF_M2TS_VIDEO_MPEG2) {
			program->pcr_pid = stream->mpeg2_es_pid;
		} else if (!program->pcr_pid) {
			program->pcr_pid = stream->mpeg2_es_pid;
		}
	}
}

MP42TS_Buffer *EncodePMT(GF_M2TS_Program *program, MP42TS_Buffer *iod)
{
	u32 i;
	MP42TS_Buffer *B;
	GF_BitStream *bs;
	GF_SAFEALLOC(B, MP42TS_Buffer);
	bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);

	gf_bs_write_int(bs,	0, 3); // reserved
	gf_bs_write_int(bs,	program->pcr_pid, 13);
	gf_bs_write_int(bs,	0, 4); // reserved
	
	if (!iod) {
		gf_bs_write_int(bs,	0, 12); // program info length =0
	} else {
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
	}	

	for (i =0; i < gf_list_count(program->streams); i++) {
		M2TS_mux_stream *es;
		es = gf_list_get(program->streams, i);
		gf_bs_write_int(bs,	es->MP2_type, 8);
		gf_bs_write_int(bs,	0, 3); // reserved
		gf_bs_write_int(bs,	es->mpeg2_es_pid, 13);
		gf_bs_write_int(bs,	0, 4); // reserved
		
		/* Second Loop Descriptor */
		if (iod) {
			gf_bs_write_int(bs,	4, 12); // ES info length = 4 :only SL Descriptor
			gf_bs_write_int(bs,	30, 8); // tag
			gf_bs_write_int(bs,	1, 8); // length
			gf_bs_write_int(bs,	es->mpeg4_es_id, 16);  // mpeg4_esid
		} else {
			gf_bs_write_int(bs,	0, 12);
		}
						
	}
	
	gf_bs_get_content(bs, &B->data, &B->length);
	
	gf_bs_del(bs);
	return B;
}

void CreateTSPacketsForPMTs(M2TS_muxer *muxer)
{
	u32 i, j, nb_progs;
	GF_M2TS_Program *program;
	M2TS_mux_stream *stream;
	MP42TS_Buffer *B;
	GF_List *sections;
	Bool use_iod = 0;
	MP42TS_Buffer *B_iod;
	
	muxer->pmt_ts_packet = gf_list_new();
	nb_progs = gf_list_count(muxer->pat_table);
	for (i= 0; i<nb_progs ;i++) {
		program = gf_list_get(muxer->pat_table,i);
		program->streams = gf_list_new();

		for (j = 0; j < gf_list_count(muxer->streams); j++) {
			stream = gf_list_get(muxer->streams, j);						
			stream->mpeg2_es_pid = 100*(i+1)+10*(j+1);			
			get_mux_stream_sample(muxer->mp4_in, stream);
			gf_list_add(program->streams, stream);
		}

		InitializePMT(program);

		if (use_iod) {
			GF_ObjectDescriptor *iod;
			GF_BitStream *bs_iod;

				/* iod */
				iod = (GF_ObjectDescriptor *)gf_isom_get_root_od(muxer->mp4_in);
				/* Mise à jour des SLConfigDescriptor à la façon MPEG-2 (timestamp_length = 33 ...) */
				if (iod->tag == GF_ODF_OD_TAG || iod->tag == GF_ODF_IOD_TAG) {
					GF_ObjectDescriptor *od = (GF_ObjectDescriptor *)iod;
					u32 i, count = gf_list_count(od->ESDescriptors);
					for (i = 0; i < count; i++ ){
						GF_ESD * esd = gf_list_get(od->ESDescriptors, i);
						esd->slConfig->AUDuration = 0;
						esd->slConfig->AULength = 0;
						esd->slConfig->AUSeqNumLength = 0;
						esd->slConfig->CUDuration = 0;
						esd->slConfig->degradationPriorityLength = 0;
						esd->slConfig->durationFlag = 0;
						esd->slConfig->hasRandomAccessUnitsOnlyFlag = 0;
						esd->slConfig->instantBitrateLength = 32;
						esd->slConfig->OCRLength = 0;
						esd->slConfig->OCRResolution = 90000;
						esd->slConfig->packetSeqNumLength = 0;
						esd->slConfig->predefined = 0;
						esd->slConfig->startCTS = 0;
						esd->slConfig->startDTS = 0;
						esd->slConfig->timeScale = 0;
						esd->slConfig->timestampLength = 33;
						esd->slConfig->timestampResolution = 90000;
						esd->slConfig->useAccessUnitEndFlag = 1;
						esd->slConfig->useAccessUnitStartFlag = 1;
						esd->slConfig->useIdleFlag = 1;
						esd->slConfig->usePaddingFlag = 0;
						esd->slConfig->useRandomAccessPointFlag = 0;
						esd->slConfig->useTimestampsFlag = 1;
					}
				}
				bs_iod = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);

				gf_odf_write_od(bs_iod, iod);
				gf_odf_desc_del((GF_Descriptor *)iod);
			
				GF_SAFEALLOC(B_iod, MP42TS_Buffer);
				gf_bs_get_content(bs_iod, &B_iod->data, &B_iod->length); 
				gf_bs_del(bs_iod);
		}

		B = EncodePMT(program, (use_iod ? B_iod: NULL));
		sections = CreateSections(B, GF_M2TS_TABLE_ID_PMT, program->number, 1);
		B = EncodeSections(sections);		
		gf_list_add(muxer->pmt_ts_packet, CreateTSPacketsFromSections(B, program->pmt_pid, NULL));
	}
}

/*************************** PAT *****************************/
GF_List *InitializePAT(u32 nb_prog) {
	GF_List *pat_table;
	GF_M2TS_Program *program;
	u32 i;

	pat_table = gf_list_new();
	for (i= 1; i<= nb_prog; i++) {
		GF_SAFEALLOC(program, GF_M2TS_Program);
		program->number= i*10;
		program->pmt_pid= i*100;
		gf_list_add(pat_table, program);
	}
	return pat_table;
}

MP42TS_Buffer *EncodePAT(GF_List *pat_table) {

	u32 i;
	MP42TS_Buffer *B_pat;
	GF_M2TS_Program *program;
	u32 nb_programs;
	
	nb_programs = gf_list_count(pat_table);

	GF_SAFEALLOC(B_pat, MP42TS_Buffer);
	B_pat->data = malloc(nb_programs * 4);

	for (i=0; i < nb_programs ;i++) {
		program = gf_list_get(pat_table, i);

		//program number
		B_pat->data[4*i] = program->number >> 8;
		B_pat->data[4*i+1] = program->number;

		// program pid < 2^13-1
		B_pat->data[4*i+2] = program->pmt_pid>> 8 | 0xe0;
		B_pat->data[4*i+3] = program->pmt_pid;
	}
	B_pat->length = i*4;

	return B_pat;	
}


void CreateTSPacketsForPAT(M2TS_muxer *muxer)
{
	MP42TS_Buffer *B;
	GF_List *sections;
	
	muxer->pat_table = InitializePAT(1 /*nb_progs*/);
	B = EncodePAT(muxer->pat_table);
	sections = CreateSections(B, GF_M2TS_TABLE_ID_PAT, 1/*transport stream id*/, 1);
	B = EncodeSections(sections);
	muxer->pat_ts_packet = CreateTSPacketsFromSections(B, 0, NULL);
}

/*************************** SDT *****************************/
MP42TS_Buffer *encodeSDT(GF_List *prog_list, char *provider_name, u32 provider_name_lenth, char *service_name, u32 service_name_lenth)
{
	u32 i, j, k, CRC;
	GF_M2TS_Program *program;
	MP42TS_Buffer *B;
	u16 desc_lenth;
	u32 nb_program = gf_list_count(prog_list);
	GF_SAFEALLOC(B, MP42TS_Buffer);
	B->length = nb_program * (10 + service_name_lenth + provider_name_lenth) + 7; // un seul descripteur: service desc 0x48
	B->data = malloc(B->length);

	B->data[0] = 0; // original_network_id (2 byte)
	B->data[1] = 0x1; 
	B->data[2] = 0; // reserved
	// program loop
	for (k= 0; k < gf_list_count(prog_list); k++){
		program = gf_list_get(prog_list,k);
		B->data[3 + k*(10 + provider_name_lenth + service_name_lenth)] = (program->number >> 8) & 0xff; // service_id
		B->data[4 + k*(10 + provider_name_lenth + service_name_lenth)] = program->number & 0xff; // service_id
		B->data[5 + k*(10 + provider_name_lenth + service_name_lenth)] = 0; // EIT

		desc_lenth = provider_name_lenth + service_name_lenth + 5; // desc_loop_lenth
		B->data[6 + k*(10 + provider_name_lenth + service_name_lenth)] = ((desc_lenth >> 8) & 0xf) | 0x80;  // dscriptor loop length : 1 desc  // 0x4 status = running
		B->data[7 + k*(10 + provider_name_lenth + service_name_lenth)] = desc_lenth & 0xff;// dscriptor loop length
		// descriptor loop
		B->data[8 + k*(10 + provider_name_lenth + service_name_lenth)] = 0x48; // desct tag
		B->data[9 + k*(10 + provider_name_lenth + service_name_lenth)] = service_name_lenth + provider_name_lenth + 3; // desc_lenth
		B->data[10 + k*(10 + provider_name_lenth + service_name_lenth)] = 0x1; // srvice type : 0x1 = digital TV
		B->data[11 + k*(10 + provider_name_lenth + service_name_lenth)] = provider_name_lenth;
		for (i = 0; i < provider_name_lenth; i++){
			B->data[12 + i + k*(10 + provider_name_lenth + service_name_lenth)] = provider_name[i];
		}
		B->data[12 + i + k*(10 + provider_name_lenth + service_name_lenth)] = service_name_lenth;
		for (j = 0; j < service_name_lenth; j++){
			B->data[13+i+j + k*(10 + provider_name_lenth + service_name_lenth)] = service_name[j];
		}
		// descriptor loop end
	}
	// crc //
	CRC = gf_m2ts_crc32(B->data,B->length-4); 
	B->data[B->length-4] = (CRC >> 24) & 0xFF;
	B->data[B->length-3] = (CRC >> 16) & 0xFF;
	B->data[B->length-2] = (CRC >> 8) & 0xFF;
	B->data[B->length-1] = CRC & 0xFF;

	return B;
}

void CreateTSPacketsForSDT(M2TS_muxer *muxer, char *provider_name, char *service_name)
{
	MP42TS_Buffer *B;
	GF_List *sections;
	B = encodeSDT(muxer->pat_table, provider_name, 4, service_name, 8);
	sections = CreateSections(B, GF_M2TS_TABLE_ID_SDT_ACTUAL, 1, 1);
	B = EncodeSections(sections);
	muxer->sdt_ts_packet = CreateTSPacketsFromSections(B, 0x11, NULL);
}


/*************************************************************************************
 * PES related functions 
 ************************************************************************************/
void add_pes_header(GF_BitStream *bs, M2TS_mux_stream *stream)
{
	u32 pes_len;
	u32 pes_header_data_length;
	Bool use_pts, use_dts;

	use_pts = 1;
	use_dts = 1;
	pes_header_data_length = 10;

	gf_bs_write_int(bs,	0x1, 24);//packet start code
	gf_bs_write_int(bs,	stream->mpeg2_pes_streamid, 8);// stream id 

	pes_len = stream->sample->dataLength + 13; // 13 = header size (including PTS/DTS)
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
		u64 cts, dts, t;
		
		cts = stream->sample->DTS + stream->sample->CTS_Offset;
		
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

		dts = stream->sample->DTS;

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

}

enum {
	M2TS_ADAPTATION_RESERVED	= 0,
	M2TS_ADAPTATION_NONE		= 1,
	M2TS_ADAPTATION_ONLY		= 2,
	M2TS_ADAPTATION_AND_PAYLOAD = 3,
};

u32 add_adaptation(GF_BitStream *bs, u32 *continuity_counter, Bool has_pcr, u64 time, u32 payload_length)
{
	u32 tmp_length;
	u32 adaptation_field_control;
	u32 adaptation_lengh;
	u32 adaptation_padding_length;

	/* length of adaptation_field_length; */ 
	u32 adaptation_length_length = 1; 
	/* discontinuty flag, random access flag ... */
	u32 adaptation_flags_length = 1; 
	/* length of encoded pcr */
	u32 pcr_length = 6;

	/* we have to write an adaptation field:
	   - to write the PCR
	   - or because we need padding */
	tmp_length = adaptation_length_length + adaptation_flags_length + has_pcr*pcr_length;
	if (has_pcr || payload_length < (184 - tmp_length)) {
		if (payload_length >= (184 - tmp_length)) {
			/* we have enough data remaining in the payload, 
			   the TS packet will have adaptation and payload (no padding) */
			adaptation_field_control = M2TS_ADAPTATION_AND_PAYLOAD;
			adaptation_lengh = tmp_length;
			adaptation_padding_length = 0;
		} else { /* padding is needed */
			if (payload_length == 0) {
				/* the TS packet will have adaptation only and padding, no payload */
				adaptation_field_control = M2TS_ADAPTATION_ONLY;
			} else {
				/* the TS packet will hava adaptation, padding and payload */
				adaptation_field_control = M2TS_ADAPTATION_AND_PAYLOAD;
			}
			adaptation_lengh = 184 - payload_length;
			adaptation_padding_length = adaptation_lengh - tmp_length;
		}
	} else {
		adaptation_field_control = M2TS_ADAPTATION_NONE;
		adaptation_lengh = 0;
		adaptation_padding_length = 0;	
	}

	gf_bs_write_int(bs,	adaptation_field_control, 2); 
	gf_bs_write_int(bs,	*continuity_counter, 4);
	if(*continuity_counter < 15 && 
		adaptation_field_control != M2TS_ADAPTATION_RESERVED && 
		adaptation_field_control != M2TS_ADAPTATION_ONLY) { 
		/* continuity counter is increased only when there is payload ? */
		*continuity_counter++;
	} else {
		*continuity_counter = 0;
	}

	if (adaptation_field_control == M2TS_ADAPTATION_ONLY || 
		adaptation_field_control == M2TS_ADAPTATION_AND_PAYLOAD) {
		u32 j;

		assert(adaptation_lengh > adaptation_length_length);

		/* the function returns the real number of bytes written, 
		   but in the bitstream we write the number of bytes, excluding the length field */
		gf_bs_write_int(bs,	adaptation_lengh - adaptation_length_length, 8);
		gf_bs_write_int(bs,	0, 1); // discontinuity indicator
		gf_bs_write_int(bs,	0, 1); // random access indicator
		gf_bs_write_int(bs,	0, 1); // es priority indicator
		gf_bs_write_int(bs,	has_pcr, 1); // PCR_flag 
		gf_bs_write_int(bs,	0, 1); // OPCR flag
		gf_bs_write_int(bs,	0, 1); // splicing point flag
		gf_bs_write_int(bs,	0, 1); // transport private data flag
		gf_bs_write_int(bs,	0, 1); // adaptation field extension flag
		if (has_pcr) {
			u64 PCR_base, PCR_ext;
			PCR_base = time;
			gf_bs_write_long_int(bs, PCR_base, 33); 
			gf_bs_write_int(bs,	0, 6); // reserved
			PCR_ext = 0;
			gf_bs_write_long_int(bs, PCR_ext, 9);
		}
		for (j=0; j<adaptation_padding_length; j++) {
			gf_bs_write_int(bs,	0xff, 8); // stuffing byte
		}
	}

	return adaptation_lengh;
}

u8 *encode_ts(M2TS_muxer *muxer, M2TS_mux_stream *stream)
{
	GF_BitStream *bs;
	u8 *ts_data;
	u32 ts_data_len;

	/* length of the header from start of pes header to start of payload (including PTS/DTS) */
	u32 pes_header_length = 19; 
	u32 remain;
	Bool PUSI;
	u32 adaptation_length;
	u32 payload_length;
	
	bs = gf_bs_new(NULL,0,GF_BITSTREAM_WRITE);

	if (stream->nb_bytes_written == 0){
		PUSI = 1;
	} else {
		PUSI = 0;
	}

	gf_bs_write_int(bs,	0x47, 8); // sync byte
	gf_bs_write_int(bs,	0, 1);    // error indicator
	gf_bs_write_int(bs,	PUSI, 1); // start ind
	gf_bs_write_int(bs,	0, 1);	  // transport priority
	gf_bs_write_int(bs,	stream->mpeg2_es_pid, 13); // pid
	gf_bs_write_int(bs,	0, 2);    // scrambling

	remain = stream->sample->dataLength + pes_header_length*PUSI - stream->nb_bytes_written;

	adaptation_length = add_adaptation(bs, &stream->continuity_counter, 
										   stream->PCR, 
										   stream->stream_time, 
										   remain);

	payload_length = 184 - adaptation_length;

	if (PUSI){
		u32 pes_payload_length;
		
		add_pes_header(bs, stream);
		pes_payload_length = payload_length - pes_header_length;
		gf_bs_write_data(bs, stream->sample->data, pes_payload_length);
		stream->nb_bytes_written += pes_payload_length;
//		fprintf(stdout, "TS payload length %d - total %d\n", pes_payload_length, stream->nb_bytes_written);
	} else {
		gf_bs_write_data(bs, stream->sample->data+stream->nb_bytes_written, payload_length);
		stream->nb_bytes_written += payload_length;
//		fprintf(stdout, "TS payload length %d - total %d \n", payload_length, stream->nb_bytes_written);
	}
	gf_bs_get_content(bs, &ts_data, &ts_data_len);
	gf_bs_del(bs);
	return ts_data;
}

void create_one_ts(GF_ISOFile *mp4_in, M2TS_mux_stream *stream, M2TS_muxer *muxer)
{
	u8 *data;	
	data = encode_ts(muxer, stream);
	fwrite(data, 1, 188, muxer->ts_out);
//	fwrite(data+4, 1, 184, stream->pes_out);
	if (stream->nb_bytes_written >= stream->sample->dataLength){		
		/* TODO free sample ? */
		stream->sample = NULL;
		get_mux_stream_sample(mp4_in, stream);
		stream->nb_bytes_written = 0;
		stream->sample_number ++;
	}
}
