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

typedef struct iso_progressive_reader {

	/* data buffer to be read by the parser */
	u8 *data;
	/* size of the data buffer */
	u32 data_size;
	/* number of valid bytes in the buffer */
	u32 valid_data_size;
	/* URL used to pass a buffer to the parser */
	char data_url[256];

	/* The ISO file structure created for the parsing of data */
	GF_ISOFile *movie;

	/* Mutex to protect the reading from concurrent adding of media data */
	GF_Mutex *mutex;

	/* Boolean indicating if the thread should stop */
	volatile Bool do_run;

	/* Boolean state to indicate if the needs to be parsed */
	Bool refresh_boxes;

	/* id of the track in the ISO to be read */
	u32 track_id;

} ISOProgressiveReader;


static u32 iso_progressive_read_thread(void *param)
{
	ISOProgressiveReader *reader = (ISOProgressiveReader *)param;
	u32 track_number;
	GF_ISOSample *iso_sample;
	u32 samples_processed;
	u32 sample_index;
	u32 sample_count;

	samples_processed = 0;
	sample_count = 0;
	track_number = 0;
	/* samples are numbered starting from 1 */
	sample_index = 1;

	while (reader->do_run == GF_TRUE) {

		/* we can only parse if there is a movie */
		if (reader->movie) {

			/* block the data input until we are done in the parsing */
			gf_mx_p(reader->mutex);

			/* get the track number we want */
			if (track_number == 0) {
				track_number = gf_isom_get_track_by_id(reader->movie, reader->track_id);
			} 

			/* only if we have the track number can we try to get the sample data */
			if (track_number != 0) {
				u32 new_sample_count;
				u32 di; /*descriptor index*/

				/* let's see how many samples we have since the last parsed */
				new_sample_count = gf_isom_get_sample_count(reader->movie, track_number);
				if (new_sample_count > sample_count) {
					/* New samples have been added to the file */
					fprintf(stdout, "Found %d new samples (total: %d)\n", new_sample_count - sample_count, new_sample_count);
					if (sample_count == 0) {
						sample_count = new_sample_count;
					}
				}
				if (sample_count == 0) {
					/* no sample yet, let the data input force a reparsing of the data */
					reader->refresh_boxes = GF_TRUE;
					/*let the reader push new data */
					gf_mx_v(reader->mutex);
					gf_sleep(1000);
				} else {
					/* we have some samples, lets keep things stable in the parser for now and 
					  don't let the data input force a reparsing of the data */
					reader->refresh_boxes = GF_FALSE;

					/* let's analyze the samples we have parsed so far one by one */
					iso_sample = gf_isom_get_sample(reader->movie, track_number, sample_index, &di);
					if (iso_sample) {
						/* if you want the sample description data, you can call:
						   GF_Descriptor *desc = gf_isom_get_decoder_config(reader->movie, reader->track_handle, di);
						*/
						
						samples_processed++;
						/*here we dump some sample info: samp->data, samp->dataLength, samp->isRAP, samp->DTS, samp->CTS_Offset */
						fprintf(stdout, "Found sample #%5d (#%5d) of length %8d, RAP: %d, DTS: "LLD", CTS: "LLD"\r\n", sample_index, samples_processed, iso_sample->dataLength, iso_sample->IsRAP, iso_sample->DTS, iso_sample->DTS+iso_sample->CTS_Offset);
						sample_index++;
						
						/*release the sample data, once you're done with it*/
						gf_isom_sample_del(&iso_sample);
						
						/* once we have read all the samples, we can release some data and force a reparse of the input buffer */
						if (sample_index > sample_count) {
							u64 new_buffer_start;
							u64 missing_bytes;

							/* release internal structures associated with the samples read so far */
							gf_isom_reset_tables(reader->movie, GF_TRUE);

#if 1
							/* release the associated input data as well */
							gf_isom_reset_data_offset(reader->movie, &new_buffer_start);
							if (new_buffer_start) {
								u32 offset = (u32)new_buffer_start;
								memmove(reader->data, reader->data+offset, reader->data_size-offset);
								reader->valid_data_size -= offset;
							} 
							sprintf(reader->data_url, "gmem://%d@%p", reader->data_size, reader->data);
							gf_isom_refresh_fragmented(reader->movie, &missing_bytes, reader->data_url, GF_TRUE);
#endif

							/* update the sample count and sample index */
							sample_count = new_sample_count - sample_count;
							assert(sample_count == 0);
							sample_index = 1;
						}
					}
					/* and finally, let the data reader push more data */
					gf_mx_v(reader->mutex);
				}
			}
		} else {
			gf_sleep(1);
		}
	}
	return 0;
}

int main(int argc, char **argv)
{
	/* The ISO progressive reader */
	ISOProgressiveReader reader;
	/* Error indicator */
	GF_Err e;
	/* input file to be read in the data buffer */
	FILE *input;
	/* number of bytes read from the file at each read operation */
	u32 read_bytes;
	/* number of bytes read from the file (total) */
	u64 total_read_bytes;
	/* size of the input file */
	u64 file_size;
	/* number of bytes required to finish the current ISO Box reading (not used here)*/
	u64 missing_bytes;
	/* Thread used to run the ISO parsing in */
	GF_Thread *reading_thread;
	/* Return value for the program */
	int ret = 0;

	/* Usage */
	if (argc != 2) {
		fprintf(stdout, "Usage: %s filename\n", argv[0]);
		return 1;
	}

	/* Initializing GPAC framework */
	/* Enables GPAC memory tracking in debug mode only */
#if defined(DEBUG) || defined(_DEBUG)
	gf_sys_init(GF_TRUE);
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
	gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
#else
	gf_sys_init(GF_FALSE);
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
#endif

	/* This is an input file to read data from. Could be replaced by any other method to retrieve the data (e.g. JavaScript, socket, ...)*/
	input = gf_f64_open(argv[1], "rb");
	if (!input) {
		fprintf(stdout, "Could not open file %s for reading.\n", argv[1]);
		gf_sys_close();
		return 1;
	}

	gf_f64_seek(input, 0, SEEK_END);
	file_size = gf_f64_tell(input);
	gf_f64_seek(input, 0, SEEK_SET);

	/* Initializing the progressive reader */
	memset(&reader, 0, sizeof(ISOProgressiveReader));
	reading_thread = gf_th_new("ISO reading thread");
	reader.mutex = gf_mx_new("ISO Segment");
	reader.do_run = GF_TRUE;
	/* we want to parse the first track */
	reader.track_id = 1;
	/* start the async parsing */
	gf_th_run(reading_thread, iso_progressive_read_thread, &reader);

	/* start the data reading */
	reader.data_size = BUFFER_BLOC_SIZE;
	reader.data = (u8 *)gf_malloc(reader.data_size);
	reader.valid_data_size = 0;
	total_read_bytes = 0;
	while (1) {
		/* block the parser until we are done manipulating the data buffer */
		gf_mx_p(reader.mutex);
		
		/* make sure we have enough space in the buffer to read the next bloc of data */
		if (reader.valid_data_size + BUFFER_BLOC_SIZE > reader.data_size) {
			reader.data = (u8 *)gf_realloc(reader.data, reader.data_size + BUFFER_BLOC_SIZE);
			reader.data_size += BUFFER_BLOC_SIZE;
		} 

		/* read the next bloc of data and update the data buffer url */
		read_bytes = fread(reader.data+reader.valid_data_size, 1, BUFFER_BLOC_SIZE, input);
		total_read_bytes += read_bytes;
		fprintf(stdout, "Read "LLD" bytes of "LLD" bytes from input file %s (buffer status: %5d/%5d)\r", total_read_bytes, file_size, argv[1], reader.valid_data_size, reader.data_size);
		if (read_bytes) {
			reader.valid_data_size += read_bytes;
			sprintf(reader.data_url, "gmem://%d@%p", reader.data_size, reader.data);
		} else {
			/* end of file we can quit */
			gf_mx_v(reader.mutex);
			break;
		}

		/* if the file is not yet opened (no movie), open it in progressive mode (to update its data later on) */
		if (!reader.movie) {
			/* let's initialize the parser */
			e = gf_isom_open_progressive(reader.data_url, 0, 0, &reader.movie, &missing_bytes);

			/* we can let parser try to work now */
			gf_mx_v(reader.mutex);

			if ((e == GF_OK || e == GF_ISOM_INCOMPLETE_FILE) && reader.movie) {
				/* nothing to do, this is normal */
			} else {
				fprintf(stdout, "Error opening fragmented mp4 in progressive mode: %s (missing "LLD" bytes)\n", gf_error_to_string(e), missing_bytes);
				gf_sys_close();
				ret = 1;
				goto exit;
			} 
		} else {
			/* let inform the parser that the buffer has been updated with new data */
			e = gf_isom_refresh_fragmented(reader.movie, &missing_bytes, reader.data_url, reader.refresh_boxes);

			/* we can let parser try to work now */
			gf_mx_v(reader.mutex);

			if (e != GF_OK && e != GF_ISOM_INCOMPLETE_FILE) {
				fprintf(stdout, "Error refreshing fragmented mp4: %s (missing "LLD" bytes)\n", gf_error_to_string(e), missing_bytes);
				gf_sys_close();
				ret = 1;
				goto exit;
			}
		}
		
		gf_sleep(1);
	}

exit:
	/* stop the parser */
	reader.do_run = GF_FALSE;
	gf_th_stop(reading_thread);

	/* clean structures */
	gf_th_del(reading_thread);
	gf_mx_del(reader.mutex);
	gf_free(reader.data);
	gf_isom_close(reader.movie);
	fclose(input);
	gf_sys_close();

	return ret;
}





