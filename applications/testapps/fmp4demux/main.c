/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2013-
 *					All rights reserved
 *
 *  This file is part of GPAC / sample MP4 demultiplexing application
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
#include <gpac/tools.h>
#include <gpac/isomedia.h>
#include <gpac/thread.h>

#define BUFFER_BLOC_SIZE 1000

typedef struct {
	/* The ISO file structure created for the parsing of data */
	GF_ISOFile *movie;

	/* Mutex to protect the reading from concurrent adding of media data */
	GF_Mutex *mutex;

	/* Boolean indicating if the thread should stop */
	Bool do_run;

	/* id of the track in the ISO to be read */
	u32 track_id;

	/* Boolean indicating that all current samples have been read and some input data can be flushed */
	Bool can_flush;

} ISOProgressiveReader;


static u32 iso_progressive_read(void *param)
{
	ISOProgressiveReader *reader = (ISOProgressiveReader *)param;
	u32 track_number;
	GF_ISOSample *iso_sample;
	u32 sample_index;
	u32 sample_count;

	track_number = 0;
	/* samples are numbered starting from 1 */
	sample_index = 1;
	while(reader->do_run == GF_TRUE) {
		if (reader->movie) {
			if (track_number == 0) {
				gf_mx_p(reader->mutex);
				track_number = gf_isom_get_track_by_id(reader->movie, reader->track_id);
				gf_mx_v(reader->mutex);
			} else {
				u32 new_sample_count;
				u32 di; /*descriptor index*/
				gf_mx_p(reader->mutex);
				new_sample_count = gf_isom_get_sample_count(reader->movie, track_number);
				if (new_sample_count > sample_count) {
					/* New data has been added to the file */
					fprintf(stdout, "Found %d new samples\n", new_sample_count - sample_count);
				}
				iso_sample = gf_isom_get_sample(reader->movie, track_number, sample_index, &di);
				gf_mx_v(reader->mutex);
				if (iso_sample) {
					/* if you want the sample description call:
					   GF_Descriptor *desc = gf_isom_get_decoder_config(reader->movie, reader->track_handle, di);
					*/
					/*here you can dump: samp->data, samp->dataLength, samp->isRAP, samp->DTS, samp->CTS_Offset */
					fprintf(stdout, "Found sample #%d of length %d, RAP: %d, DTS: "LLD", CTS: "LLD"\n", sample_index, iso_sample->dataLength, iso_sample->IsRAP, iso_sample->DTS, iso_sample->DTS+iso_sample->CTS_Offset);
					gf_isom_sample_del(&iso_sample);
					sample_index++;
					if (sample_index == sample_count) {
						gf_mx_p(reader->mutex);
						reader->can_flush = GF_TRUE;
						gf_mx_v(reader->mutex);
						sample_count = new_sample_count - sample_count;
					}
				}
			}
		} else {
			gf_sleep(1000);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	ISOProgressiveReader reader;
	/* Error indicator */
	GF_Err e;
	/* variable used for the dummy input file reading */
	u8 *data;
	/* size of the data buffer */
	u32 data_size;
	/* number of valid bytes in the buffer */
	u32 valid_data_size;
	/* offset in the data from where the parser reads */
	u32 last_data_offset;
	/* input file to be read in the data buffer */
	FILE *input;
	/* number of bytes read at each read operation */
	u32 read_bytes;
	/* number of bytes required to finish the current ISO Box reading */
	u64 missing_bytes;
	/* URL used to pass a buffer to the parser */
	char data_url[256];
	/* Thread used to run the ISO parsing in */
	GF_Thread *reading_thread;

	/* Initializing GPAC framework */
	gf_sys_init(GF_FALSE);
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);

	/* This is an input file to read data from. Could be replaced by any other method to retrieve the data (e.g. JavaScript, socket, ...)*/
	input = gf_f64_open(argv[1], "rb");
	if (!input) return 0;

	memset(&reader, 0, sizeof(ISOProgressiveReader));
	reading_thread = gf_th_new("ISO reading thread");
	reader.mutex = gf_mx_new("ISO Segment");
	reader.do_run = GF_TRUE;
	/* we want to parse the first track */
	reader.track_id = 1;
	gf_th_run(reading_thread, iso_progressive_read, &reader);

	data_size = BUFFER_BLOC_SIZE;
	data = (u8 *)gf_malloc(data_size);
	last_data_offset = 0;
	valid_data_size = 0;
	while(1) {
		/* Check if we can flush some input data, no longer needed by the parser */
		if (reader.can_flush) {
			gf_mx_p(reader.mutex);
			memmove(data, data+last_data_offset, data_size-last_data_offset);
			valid_data_size -= last_data_offset;
			last_data_offset = 0;
			reader.can_flush = GF_FALSE;
			gf_mx_v(reader.mutex);
		} 
		
		/* make sure we have enough space in the buffer to read the next bloc of data */
		if (valid_data_size + BUFFER_BLOC_SIZE > data_size) {
			data = (u8 *)gf_realloc(data, data_size + BUFFER_BLOC_SIZE);
			data_size += BUFFER_BLOC_SIZE;
		}

		/* read the next bloc of data and update the blob url */
		read_bytes = fread(data+valid_data_size, 1, BUFFER_BLOC_SIZE, input);
		if (read_bytes) {
			valid_data_size += read_bytes;
			sprintf(data_url, "gmem://%d@%p",  data_size, data);
		}

		/* if the file is not yet opened (no movie), open it in progressive mode (to update its data later on) */
		if (!reader.movie) {
			gf_mx_p(reader.mutex);
			e = gf_isom_open_progressive(data_url, 0, 0, &reader.movie, &missing_bytes/*, &last_data_offset*/);
			gf_mx_v(reader.mutex);
			fprintf(stdout, "Opening file in progressive mode: missing "LLD" bytes\n", missing_bytes);

			if ((e == GF_OK || e == GF_ISOM_INCOMPLETE_FILE) && reader.movie) {
				/* nothing to do */
			} else {
				fprintf(stdout, "Error opening fragmented mp4 in progressive mode: %s", gf_error_to_string(e));
				return 0;
			} 
		} else {
			/* we can inform the parser that the buffer has been updated with new data */
			gf_mx_p(reader.mutex);
			e = gf_isom_refresh_fragmented(reader.movie, &missing_bytes, data_url);
			fprintf(stdout, "Updating file in progressive mode: missing "LLD" bytes\n", missing_bytes);
			gf_mx_v(reader.mutex);
			if (e != GF_OK) {
				fprintf(stdout, "Error refreshing fragmented mp4: %s", gf_error_to_string(e));
			}
		}
	}
	reader.do_run = GF_FALSE;
	fclose(input);
	gf_free(data);
	gf_sys_close();

	return GF_OK;
}





