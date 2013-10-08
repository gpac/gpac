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

int main(int argc, char **argv)
{
	/* The ISO progressive reader */
	GF_ISOFile *movie;
	/* Error indicator */
	GF_Err e;
	/* number of bytes required to finish the current ISO Box reading (not used here)*/
	u64 missing_bytes;
	/* Return value for the program */
	int ret = 0;
	u32 track_id = 1;
	u32 track_number;
	u32 sample_count;
	u32 sample_index;

	/* Usage */
	if (argc < 2) {
		fprintf(stdout, "Usage: %s filename [track_id]\n", argv[0]);
		return 1;
	}
	if (argc == 3) {
		track_id = atoi(argv[2]);
	}

	e = gf_isom_open_progressive(argv[1], 0, 0, &movie, &missing_bytes);
	if ((e != GF_OK && e != GF_ISOM_INCOMPLETE_FILE) || movie == NULL) {
		fprintf(stdout, "Could not open file %s for reading (%s).\n", argv[1], gf_error_to_string(e));
		return 1;
	}

	track_number = gf_isom_get_track_by_id(movie, track_id);
	if (track_number == 0) {
		fprintf(stdout, "Could not open file %s for reading (%s).\n", argv[1], gf_error_to_string(e));
		ret = 1;
		goto exit;
	}

	sample_count = 	gf_isom_get_sample_count(movie, track_number);
	sample_index = 1;
	while (sample_index <= sample_count) {
		GF_ISOSample *iso_sample;
		u32 sample_description_index;

		iso_sample = gf_isom_get_sample(movie, track_number, sample_index, &sample_description_index);
		if (iso_sample) {
			fprintf(stdout, "Found sample #%5d (#%5d) of length %8d, RAP: %d, DTS: "LLD", CTS: "LLD"\r", sample_index, sample_count, iso_sample->dataLength, iso_sample->IsRAP, iso_sample->DTS, iso_sample->DTS+iso_sample->CTS_Offset);
			sample_index++;
						
			/*release the sample data, once you're done with it*/
			gf_isom_sample_del(&iso_sample);
		} else {
			e = gf_isom_refresh_fragmented(movie, &missing_bytes, argv[1]);
		}
	}

exit:
	gf_isom_close(movie);

	return ret;
}
