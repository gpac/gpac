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

void analyse_mp4_file(M2TS_muxer *muxer)
{
	M2TS_mux_stream *stream;
	u32 i, nb_track; 
	
	nb_track = gf_isom_get_track_count(muxer->mp4_in);
	for (i=1; i<=nb_track; i++) {
		GF_SAFEALLOC(stream, M2TS_mux_stream);
		stream->track_number = i;
		stream->mpeg4_es_id = gf_isom_get_track_id(muxer->mp4_in, i);
		stream->MP4_type = gf_isom_get_media_type(muxer->mp4_in, i);
		stream->nb_sample = gf_isom_get_sample_count(muxer->mp4_in, i);

		switch (stream->MP4_type) {
			case GF_ISOM_MEDIA_VISUAL:
				if (muxer->use_sl) {
					stream->MP2_type = GF_M2TS_SYSTEMS_MPEG4_PES;
				} else {
					stream->MP2_type = GF_M2TS_VIDEO_MPEG2;
				}
				break;
			case GF_ISOM_MEDIA_AUDIO:
				if (muxer->use_sl) {
					stream->MP2_type = GF_M2TS_SYSTEMS_MPEG4_PES;
				} else {
					stream->MP2_type = GF_M2TS_AUDIO_MPEG2;
				}
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

void m2ts_write_ts_packet(M2TS_muxer *muxer, unsigned char *data, u32 *pid_found)
{
	u32 i, pid;
	M2TS_pid_list *pid_list = muxer->user;
	
	pid = ((data[1]&0x1f) << 8) | data[2];
	for (i=0; i<pid_list->length; i++){
		if (pid == pid_list->pid[i]){
			fwrite(data, 1, 188, muxer->ts_out);
			*pid_found = 1;
			break;
		}
	}
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
			stream = compare_muxer_time_with_stream_time(muxer);
			if (muxer->end) return;

			if ((stream->SL_in_pes || stream->SL_in_section) && stream->sample){
				muxer->on_event(muxer, GF_M2TS_EVT_SL_SECTION, stream);
				return;
			}

			create_one_ts(muxer->mp4_in, stream, muxer);
			
			break;

		case GF_M2TS_EVT_SL_SECTION:
			
			stream = par;
			if (!stream->sl_packet && stream->sample){
				/* AU ---> SL : 1 sample = 1paquet SL*/
				if (stream->SL_in_section){
					static u32 writeSL = 1;
					u32 count;
					/* créer une section ou plus pour ce paquet SL => list de paquet TS*/
					printf("sample number %d: DTS "LLD", CTS Offset %d\n", stream->sample_number, stream->sample->DTS, stream->sample->CTS_Offset);			
										
					config_sample_hdr(stream);
					gf_sl_packetize(stream->SLConfig, stream->SLHeader, stream->sample->data, stream->sample->dataLength, &stream->sl_packet, &stream->sl_packet_len);

					
					writeSL++;
					stream->sl_section_ts_packets = CreateTSPacketsFromSLPacket(stream, stream->sl_packet, stream->sl_packet_len);

					count = gf_list_count(stream->sl_section_ts_packets);
					printf("sl_packet_length %d ,nb ts packet %d \n", stream->sl_packet_len, count);

				}else if (stream->SL_in_pes && (stream->nb_bits_written_from_sample == 0)){
					/* encapsuler le paquet SL dans un PES */
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
					stream->nb_bits_written_from_sample = 0;
					stream->sample->dataLength = 0;
					free(stream->sample->data);
					update_muxer(muxer, muxer->mp4_in, stream);
				}

			}else if (stream->SL_in_pes && stream->sample->data){
				
				create_one_ts(muxer->mp4_in, stream, muxer);
				if (stream->nb_bits_written_from_sample == 0){
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
					/*read one ts packet*/
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

						/*process ts packet*/
						//	gf_m2ts_process_data(ts, data, data_size);
						m2ts_write_ts_packet(muxer, data, &pid_found);
					}
					pos = gf_bs_get_position(mp2_bs);
					if (pid_found == 1) break;
				}
			}
			break;
	}
}

void mpeg_ts_in(M2TS_muxer *muxer)
{
	M2TS_pid_list *pid_list;
	GF_SAFEALLOC(pid_list, M2TS_pid_list);

	pid_list->length = 8; // PAT, 2PMT, SDT, 4ES
	pid_list->pid = malloc(pid_list->length*4);
	
	pid_list->pid[0] = 110;//pmt 1
	pid_list->pid[1] = 410;//pmt 2
	pid_list->pid[2] = 0x11;//sdt 1
	pid_list->pid[3] = 120;//ES 1 
	pid_list->pid[4] = 130;//ES 2
	pid_list->pid[5] = 420;//ES 3
	pid_list->pid[6] = 430;//ES 4

	pid_list->pid[7] = 0; //pat
	muxer->user = pid_list;
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
			initialize_mux_stream(muxer->mp4_in, stream);
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

void initialize_SDT(M2TS_muxer *muxer, GF_List *pat_table, char *provider_name, char *service_name)
{
	MP42TS_Buffer *B;
	GF_List *sections;
	B = encodeSDT(pat_table, provider_name, 4, service_name, 8);
	sections = CreateSections(B, GF_M2TS_TABLE_ID_SDT_ACTUAL, 1, 1);
	B = EncodeSections(sections);
	muxer->sdt_ts_packet = CreateTSPacketsFromSections(B, 0x11, NULL);
}

void usage() 
{
	fprintf(stderr, "usage: mp42ts input.mp4 output.ts\n");
}

void main(int argc, char **argv)
{
	M2TS_muxer *muxer;
	u32 send_pat, send_pmt;

	if (argc < 2) {
		usage();
		return;
	}

	gf_sys_init();

	GF_SAFEALLOC(muxer, M2TS_muxer);
	initialize_muxer(muxer);
	muxer->on_event = M2TS_OnEvent_muxer;

	/* Open mpeg2ts file*/
	muxer->ts_out = fopen(argv[2], "wb");
	if (!muxer->ts_out) {
		fprintf(stderr, "Error opening %s\n", argv[2]);
		goto err;
	}

	/* Open ISO file  */
	if (gf_isom_probe_file(argv[1])) 
		muxer->mp4_in = gf_isom_open(argv[1], GF_ISOM_OPEN_READ, 0);
	else {
		fprintf(stderr, "Error opening %s - not an ISO media file\n", argv[1]);
		goto err;
	}

	muxer->use_sl = 0;
	analyse_mp4_file(muxer);
	
	CreateTSPacketsForPAT(muxer);
	muxer->on_event(muxer, GF_M2TS_EVT_PAT, NULL);
	fflush(muxer->ts_out);

	CreateTSPacketsForPMTs(muxer);
	muxer->on_event(muxer, GF_M2TS_EVT_PMT, NULL);
	fflush(muxer->ts_out);
	
/*	initialize_SDT(muxer, pat_table, "GPAC", "GPAC Service");
	muxer->on_event(muxer, GF_M2TS_EVT_SDT, NULL);*/

	muxer->TS_Rate      = 512000;	// bps
	muxer->PAT_interval = 0.500;	// s
	muxer->PMT_interval = 0.500;	// s
	muxer->SI_interval  = 1;		// s

	/* nb TS packet before sending new PAT */
	send_pat = (u32)(muxer->TS_Rate * muxer->PAT_interval / 188);  
	/* nb TS packet before sending new PMT */
	send_pmt = (u32)(muxer->TS_Rate * muxer->PMT_interval / 188);

	while (!muxer->end) {
		/*if (muxer->insert_SI == send_pat){
			muxer->on_event(muxer, GF_M2TS_EVT_PAT, NULL);
			// muxer->on_event(muxer, GF_M2TS_EVT_SDT, NULL); 
		} else if (muxer->insert_SI == send_pmt){
			muxer->on_event(muxer, GF_M2TS_EVT_PMT, NULL);
		} */
		muxer->on_event(muxer, GF_M2TS_EVT_ES, NULL);		
	}
	
	gf_isom_close(muxer->mp4_in);
	if (muxer->ts_out)	fclose(muxer->ts_out);

err:
	free(muxer);
}