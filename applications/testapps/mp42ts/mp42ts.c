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

#if 0
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
	CRC = gf_crc_32(B->data,B->length-4); 
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

	if (stream->muxer->log_level == LOG_PES) fprintf(stdout, "PID %d - Starting PES for AU %d - DTS "LLD" - PTS "LLD" - RAP %d\n", stream->mpeg2_es_pid, stream->sample_number, stream->sample->DTS, stream->sample->DTS+stream->sample->CTS_Offset, stream->sample->IsRAP);
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
	Bool is_rap;
	
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

	if (stream->MP2_type == GF_M2TS_VIDEO_MPEG2 && stream->sample->IsRAP) is_rap=1;
	else is_rap = 0;

	adaptation_length = add_adaptation(bs, &stream->continuity_counter, 
										   stream->PCR, 
										   stream->stream_time,
										   is_rap,
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

void create_one_ts(M2TS_muxer *muxer, M2TS_mux_stream *stream)
{
	u8 *data;	

	if (!stream->sample || stream->nb_bytes_written >= stream->sample->dataLength){		
		u32 tmp;
		/* TODO free sample ? */
		stream->sample = NULL;
		stream->sample_number ++;
		stream->sample = gf_isom_get_sample(muxer->mp4_in, stream->track_number, stream->sample_number, &tmp);
		if (stream->sample) stream->stream_time = stream->sample->DTS;
		stream->nb_bytes_written = 0;
	}

	if (stream->sample) {
		data = encode_ts(muxer, stream);
		fwrite(data, 1, 188, muxer->ts_out);
		//fwrite(data+4, 1, 184, stream->pes_out);
	}
}

void get_mux_stream_sample(GF_ISOFile *mp4_in, M2TS_mux_stream *stream);

void initialize_muxer(M2TS_muxer *muxer)
{
	M2TS_mux_stream *stream;
	u32 i, nb_track; 
	
	muxer->muxer_time = 0;
	muxer->streams = gf_list_new();
	muxer->pmt_ts_packet = gf_list_new();

	nb_track = gf_isom_get_track_count(muxer->mp4_in);
	for (i=1; i<=nb_track; i++) {
		char out[500];
		GF_SAFEALLOC(stream, M2TS_mux_stream);
		stream->muxer = muxer;
		stream->track_number = i;
		stream->mpeg4_es_id = gf_isom_get_track_id(muxer->mp4_in, i);
		stream->MP4_type = gf_isom_get_media_type(muxer->mp4_in, i);
		stream->nb_samples = gf_isom_get_sample_count(muxer->mp4_in, i);
		if (0) {
			sprintf(out, "out%i.pes", i);
			stream->pes_out = fopen(out, "wb");
		}

		switch (stream->MP4_type) {
			case GF_ISOM_MEDIA_VISUAL:
				if (muxer->use_sl) {
					stream->MP2_type = GF_M2TS_SYSTEMS_MPEG4_PES;
				} else {
					stream->MP2_type = GF_M2TS_VIDEO_MPEG2;
				}
				stream->mpeg2_pes_streamid = 0xE0 + i;
				break;
			case GF_ISOM_MEDIA_AUDIO:
				if (muxer->use_sl) {
					stream->MP2_type = GF_M2TS_SYSTEMS_MPEG4_PES;
				} else {
					stream->MP2_type = GF_M2TS_AUDIO_MPEG2;
				}
				stream->mpeg2_pes_streamid = 0xC0 + i;
				break;
			case GF_ISOM_MEDIA_OD:
				stream->MP2_type = GF_M2TS_SYSTEMS_MPEG4_SECTIONS;
				break;
			case GF_ISOM_MEDIA_SCENE:
				stream->MP2_type = GF_M2TS_SYSTEMS_MPEG4_SECTIONS;
				break;
			default:
				fprintf(stderr, "Track type %d not supported in MPEG-2\n", stream->MP4_type);
		}

		if (stream->MP2_type == GF_M2TS_SYSTEMS_MPEG4_PES || 
			stream->MP2_type == GF_M2TS_SYSTEMS_MPEG4_SECTIONS) {
			stream->SLConfig = (GF_SLConfig *)gf_odf_desc_new(GF_ODF_SLC_TAG);
			stream->SLConfig->AUDuration = 0;
			stream->SLConfig->AULength = 0;
			stream->SLConfig->AUSeqNumLength = 0;
			stream->SLConfig->CUDuration = 0;
			stream->SLConfig->degradationPriorityLength = 0;
			stream->SLConfig->durationFlag = 0;
			stream->SLConfig->hasRandomAccessUnitsOnlyFlag = 0;
			stream->SLConfig->instantBitrateLength = 32;
			stream->SLConfig->OCRLength = 0;
			stream->SLConfig->OCRResolution = 90000;
			stream->SLConfig->packetSeqNumLength = 0;
			stream->SLConfig->predefined = 0;
			stream->SLConfig->startCTS = 0;
			stream->SLConfig->startDTS = 0;
			stream->SLConfig->timeScale = 0;
			stream->SLConfig->timestampLength = 33;
			stream->SLConfig->timestampResolution = 90000;
			stream->SLConfig->useAccessUnitEndFlag = 1;
			stream->SLConfig->useAccessUnitStartFlag = 1;
			stream->SLConfig->useIdleFlag = 1;
			stream->SLConfig->usePaddingFlag = 0;
			stream->SLConfig->useRandomAccessPointFlag = 0;
			stream->SLConfig->useTimestampsFlag = 1;
		}
		if (stream->MP2_type == GF_M2TS_SYSTEMS_MPEG4_PES){
			stream->SL_in_pes = 1;
		} else if (stream->MP2_type == GF_M2TS_SYSTEMS_MPEG4_SECTIONS){
			stream->SL_in_section = 1;
			stream->sl_section_ts_packets = gf_list_new();
		}
		gf_list_add(muxer->streams, stream);
	}
}

/*********************************************************************************
 * Muxer general functions 
 ********************************************************************************/
M2TS_mux_stream *get_current_stream(M2TS_muxer *muxer)
{
	M2TS_mux_stream *stream;
	M2TS_mux_stream *res;
	u32 i, count;
	u64 inf_time = 0xffffffffffffffff;

	count = gf_list_count(muxer->streams);
	for(i = 0; i < count; i++) {
		stream = gf_list_get(muxer->streams, i);
		if (stream->stream_time < inf_time) { 
			res = stream;
			inf_time = stream->stream_time;
		}
	}

	if (res->sample_number > res->nb_samples) muxer->end = 1;
	return stream;
}

M2TS_mux_stream *compare_muxer_time_with_stream_time(M2TS_muxer *muxer)
{
	M2TS_mux_stream *stream;
	u32 i, k, count;
	u64 inf_time = 0xffffffffffffffff;
	u32 stream_with_inf_time = 0;

	k=0;
	count = gf_list_count(muxer->streams);
	for(i = 0; i < count; i++) {
		stream = gf_list_get(muxer->streams, i);
		if (stream->sample) {
			if (stream->stream_time < inf_time) {
				inf_time = stream->stream_time;
				stream_with_inf_time = i; 
			}
		} else {
			u32 SI_index = (u32)(muxer->TS_Rate * muxer->SI_interval / 188);
			if (stream->SL_in_section && stream->repeat_section && 
				muxer->insert_SI == SI_index){
				// par exemple pour qu'une image soit retransmise
				stream->nb_bytes_written = 0;  
				update_muxer(muxer, muxer->mp4_in, stream);
			}
			k++;
		}
	}

	if (k == i) muxer->end = 1;
	stream = gf_list_get(muxer->streams, stream_with_inf_time);
	muxer->muxer_time = stream->stream_time;
	return stream; 
}

void M2TS_OnEvent_muxer(M2TS_muxer *muxer, u32 evt_type, void *par)
{
	M2TS_mux_stream *stream;

	switch (evt_type) {
		case GF_M2TS_EVT_PAT:
			{
				u8 *data;
				u32 i, count;
				count = gf_list_count(muxer->pat_ts_packet);
				for (i = 0; i < count; i++) {
					data = gf_list_get(muxer->pat_ts_packet, i);
					fwrite(data, 1, 188, muxer->ts_out);
				}
			}
			break;

		case GF_M2TS_EVT_SDT:
			{
				u8 *data;
				u32 i, count;
				count = gf_list_count(muxer->sdt_ts_packet);
				for (i = 0; i < count; i++) {
					data = gf_list_get(muxer->sdt_ts_packet, i);
					fwrite(data, 1, 188, muxer->ts_out);
				}
			}
			break;

		case GF_M2TS_EVT_PMT:
			{
				u32 i, j, count1, count2;
				u8 *data;
				GF_List *ts_packet;

				count1 = gf_list_count(muxer->pmt_ts_packet);
				for (i = 0; i < count1; i++) {
					ts_packet = gf_list_get(muxer->pmt_ts_packet, i);
					count2 = gf_list_count(ts_packet);
					for (j = 0; j<count2; j++) {
						data = gf_list_get(ts_packet, j);
						fwrite(data, 1, 188, muxer->ts_out);
					}
				}
				muxer->insert_SI = 0;
			}			
			break;

		case GF_M2TS_EVT_ES:
			stream = get_current_stream(muxer); //compare_muxer_time_with_stream_time(muxer);
			if (muxer->end) return;
			create_one_ts(muxer, stream);
			break;

/*
		case GF_M2TS_EVT_SL_SECTION:
			
			stream = par;
			if (!stream->sl_packet && stream->sample){
				// AU ---> SL : 1 sample = 1paquet SL
				if (stream->SL_in_section){
					static u32 writeSL = 1;
					u32 count;
					// créer une section ou plus pour ce paquet SL => list de paquet TS
					printf("sample number %d: DTS "LLD", CTS Offset %d\n", stream->sample_number, stream->sample->DTS, stream->sample->CTS_Offset);			
										
					config_sample_hdr(stream);
					gf_sl_packetize(stream->SLConfig, stream->SLHeader, stream->sample->data, stream->sample->dataLength, &stream->sl_packet, &stream->sl_packet_len);

					
					writeSL++;
					stream->sl_section_ts_packets = CreateTSPacketsFromSLPacket(stream, stream->sl_packet, stream->sl_packet_len);

					count = gf_list_count(stream->sl_section_ts_packets);
					printf("sl_packet_length %d ,nb ts packet %d \n", stream->sl_packet_len, count);

				}else if (stream->SL_in_pes && (stream->nb_bytes_written == 0)){
					config_sample_hdr(stream);
					gf_sl_packetize(stream->SLConfig, stream->SLHeader, stream->sample->data, stream->sample->dataLength, &stream->sample->data, &stream->sample->dataLength);
						
				}

			}
			if (stream->SL_in_section && stream->sl_packet){

				unsigned char *data;
				static nb_ts_sec = 0;
				data = gf_list_get(stream->sl_section_ts_packets, 0);
				fwrite(data, 1, 188, muxer->ts_out);
				nb_ts_sec ++;
				gf_list_rem(stream->sl_section_ts_packets, 0);
				
				
				if (gf_list_count(stream->sl_section_ts_packets) == 0){
					
					printf("nb ts write %d\n\n", nb_ts_sec);
					nb_ts_sec = 0;

					stream->sl_packet = NULL;
					stream->sl_packet_len = 0;
					stream->nb_bytes_written = 0;
					stream->sample->dataLength = 0;
					free(stream->sample->data);
					update_muxer(muxer, muxer->mp4_in, stream);
				}

			}else if (stream->SL_in_pes && stream->sample->data){
				
				create_one_ts(muxer, stream);
				if (stream->nb_bytes_written == 0){
					stream->sl_packet = NULL;
					stream->sl_packet_len = 0;
				}
			}
		

			break;

		case GF_M2TS_EVT_DEMUX_DATA:
			{
				GF_BitStream *mp2_bs;
				u32 pid_found = 0;
				u64 pos, has_data;
				mp2_bs = par;

				has_data = gf_bs_available(mp2_bs);
				while (has_data > 0) {
					u32 i = 0;
					unsigned char *data;
					u32 data_size = 188;
					data = malloc(data_size);
					// read one ts packet
					gf_bs_read_data(mp2_bs, data, data_size);
					has_data = gf_bs_available(mp2_bs);
					pos = gf_bs_get_position(mp2_bs);
					if (data[0] != 0x47){
						u32 k = 0;
						i++;
						printf("Sync byte error %d\r", i);
						while (data[0] != 0x47 && has_data){
							data = &data[1];
							data[187] = gf_bs_read_u8(mp2_bs);
							k++;
						}
					}else{

						//	gf_m2ts_process_data(ts, data, data_size);
						m2ts_write_ts_packet(muxer, data, &pid_found);
					}
					pos = gf_bs_get_position(mp2_bs);
					if (pid_found == 1) break;
				}
			}
			break;*/
	}
}


/*Events used by the MPEGTS muxer*/
enum
{
	GF_M2TS_EVT_PAT = 0,
	GF_M2TS_EVT_PMT,		
	GF_M2TS_EVT_SDT,		
	GF_M2TS_EVT_ES,
	GF_M2TS_EVT_SL_SECTION,
	/* Data from demuxer */
	GF_M2TS_EVT_DEMUX_DATA,
};

typedef struct
{
	u8 *data;
	u32 length;
} MP42TS_Buffer;

typedef struct M2TS_mux_stream
{
	u32 track_number;
	u32 mpeg2_es_pid;
	u32 mpeg2_pes_streamid;
	u32 mpeg4_es_id;

	GF_ISOSample *sample;
	u32 sample_number;
	u32 nb_samples;
	u32 nb_bytes_written;
	u32 continuity_counter;

	GF_List *packets;


	GF_SLConfig *SLConfig;
	GF_SLHeader *SLHeader;	
	Bool SL_in_pes;
	u8 *sl_packet;
	u32 sl_packet_len;
	Bool SL_in_section;
	Bool repeat_section;
	u32 SL_section_version_number;
	GF_List *sl_section_ts_packets;
	
	GF_M2TS_PES *PES;

	u8 is_time_initialized;
	u64 stream_time;
	
	u32 MP2_type;
	u32 MP4_type;

	FILE *pes_out;

	Bool PCR;

	struct M2TS_muxer *muxer;
} M2TS_mux_stream;

typedef struct M2TS_muxer
{
	u32 TS_Rate;  // bps
	float PAT_interval; // s
	float PMT_interval; // s
	float SI_interval; //s
	
	u32 send_pat, send_pmt;

	/* ~ PCR */
	u64 muxer_time;
	
	/* M2TS_mux_stream List*/
	GF_List *streams;

	GF_List *pat_table; /* List of GF_M2TS_Program */

	GF_List *pat_ts_packet; /* List of Encoded TS packets corresponding to PAT */
	GF_List *sdt_ts_packet; /* List of Encoded TS packets corresponding to SDT */
	GF_List *pmt_ts_packet; /* List of List of Encoded TS packets corresponding to each PMT */

	/*user callback - MUST NOT BE NULL*/
	void (*on_event)(struct M2TS_muxer *muxer, u32 evt_type, void *par);
	/*private user data*/
	void *user;

	GF_ISOFile *mp4_in;
	FILE *ts_out;

	u32 insert_SI;
	Bool end;

	Bool use_sl;

	u32 log_level;
} M2TS_muxer;

typedef struct
{
	u32 *pid;
	u32 length;
} M2TS_pid_list;
#endif
