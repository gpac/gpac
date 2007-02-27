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
				stream->mpeg2_pes_streamid = 0xC0 + i;
				break;
			case GF_ISOM_MEDIA_AUDIO:
				if (muxer->use_sl) {
					stream->MP2_type = GF_M2TS_SYSTEMS_MPEG4_PES;
				} else {
					stream->MP2_type = GF_M2TS_AUDIO_MPEG2;
				}
				stream->mpeg2_pes_streamid = 0xE0 + i;
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

/*********************************************************************************
 * Muxer general functions 
 ********************************************************************************/
void add_stream_to_muxer(M2TS_muxer *muxer, M2TS_mux_stream *stream)
{	
	if (stream->stream_time < muxer->muxer_time ) muxer->muxer_time = stream->stream_time;
	gf_list_add(muxer->streams, stream);
}

void get_mux_stream_sample(GF_ISOFile *mp4_in, M2TS_mux_stream *stream)
{
	u32 ivar;
#if 0
	GF_Err e;
	u32 isn;	

	if (!stream->is_time_initialized) {
		e = gf_isom_get_sample_for_movie_time(mp4_in, stream->track_number, 0, &ivar, GF_ISOM_SEARCH_FORWARD, &stream->sample, &isn);		
		stream->is_time_initialized = 1;
		stream->sample_number ++;	
	} else {
		e = gf_isom_get_sample_for_media_time(mp4_in, stream->track_number, stream->stream_time + 1, &ivar, GF_ISOM_SEARCH_FORWARD, &stream->sample, &isn);
	}
#else
	stream->sample = gf_isom_get_sample(mp4_in, stream->track_number, stream->sample_number, &ivar);
#endif
	if (stream->sample) stream->stream_time = stream->sample->DTS;
}

void update_muxer(M2TS_muxer *muxer, GF_ISOFile *mp4_in, M2TS_mux_stream *stream)
{
	if (stream->nb_bytes_written >= stream->sample->dataLength){		
		/* TODO free sample ? */
		stream->sample = NULL;
		get_mux_stream_sample(mp4_in, stream);
		stream->nb_bytes_written = 0;
		stream->sample_number ++;
	}
	muxer->insert_SI ++;
}

M2TS_mux_stream *get_current_stream(M2TS_muxer *muxer)
{
	M2TS_mux_stream *stream;
	stream = gf_list_get(muxer->streams, 0);
	if (stream->sample_number > stream->nb_samples) muxer->end = 1;
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

void usage() 
{
	fprintf(stderr, "usage: mp42ts input.mp4 output.ts\n");
}

void main(int argc, char **argv)
{
	M2TS_muxer muxer;

	if (argc < 2) {
		usage();
		return;
	}

	gf_sys_init();

	memset(&muxer, 0, sizeof(M2TS_muxer));
	muxer.on_event = M2TS_OnEvent_muxer;
	
	/* Open mpeg2ts file*/
	muxer.ts_out = fopen(argv[2], "wb");
	if (!muxer.ts_out) {
		fprintf(stderr, "Error opening %s\n", argv[2]);
		return;
	}

	/* Open ISO file  */
	if (gf_isom_probe_file(argv[1])) 
		muxer.mp4_in = gf_isom_open(argv[1], GF_ISOM_OPEN_READ, 0);
	else {
		fprintf(stderr, "Error opening %s - not an ISO media file\n", argv[1]);
		return;
	}

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
	if (muxer.ts_out) fclose(muxer.ts_out);
}