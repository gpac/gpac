/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Romain Bouqueau - Cyril Concolato
 *			Copyright (c) Romain Bouqueau 2013-
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

static void process_samples_from_track(GF_ISOFile *movie, u32 track_id)
{
	u32 track_number;
	u32 sample_count;
	u32 sample_index;
	/* Error indicator */
	GF_Err e;
	/* Number of bytes required to finish the current ISO Box reading */
	u64 missing_bytes;

	track_number = gf_isom_get_track_by_id(movie, track_id);
	if (track_number == 0) {
		fprintf(stdout, "Could not find track ID=%u. Ignore segment.\n", track_id);
		return;
	}

	sample_count = gf_isom_get_sample_count(movie, track_number);
	sample_index = 1;
	while (sample_index <= sample_count) {
		GF_ISOSample *iso_sample;
		u32 sample_description_index;

		iso_sample = gf_isom_get_sample(movie, track_number, sample_index, &sample_description_index);
		if (iso_sample) {
			fprintf(stdout, "Found sample #%5d/%5d of length %8d, RAP: %d, DTS: "LLD", CTS: "LLD"\n", sample_index, sample_count, iso_sample->dataLength, iso_sample->IsRAP, iso_sample->DTS, iso_sample->DTS+iso_sample->CTS_Offset);
			sample_index++;

			/* Release the sample data, once you're done with it*/
			gf_isom_sample_del(&iso_sample);
		} else {
			e = gf_isom_last_error(movie);
			if (e == GF_ISOM_INCOMPLETE_FILE) {
				missing_bytes = gf_isom_get_missing_bytes(movie, track_number);
				fprintf(stdout, "Missing "LLU" bytes on input file\n", missing_bytes);
				gf_sleep(1000);
			}
		}
	}
}

int main(int argc, char **argv)
{
	/* The ISO progressive reader */
	GF_ISOFile *movie;
	/* Error indicator */
	GF_Err e;
	/* Number of bytes required to finish the current ISO Box reading */
	u64 missing_bytes;
	/* Return value for the program */
	int ret = 0;
	/* Maximum index of the segments*/
	u32 seg_max = argc-2;
	/* Number of the segment being processed*/
	u32 seg_curr = 0;
	u32 track_id = 1;

	/* Usage */
	if (argc < 2) {
		fprintf(stdout, "Usage: %s filename0 [filename1 filename2 ...]\n", argv[0]);
		return 1;
	}

#if defined(DEBUG) || defined(_DEBUG)
	/* Enables GPAC memory tracking in debug mode only */
	gf_sys_init(GF_TRUE);
	gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
#endif

	/* First or init segment */
	fprintf(stdout, "Process segment %5d/%5d: %s\n", seg_curr, seg_max, argv[seg_curr+1]);
	e = gf_isom_open_progressive(argv[seg_curr+1], 0, 0, &movie, &missing_bytes);
	if ((e != GF_OK && e != GF_ISOM_INCOMPLETE_FILE) || movie == NULL) {
		fprintf(stdout, "Could not open file %s for reading (%s).\n", argv[seg_curr+1], gf_error_to_string(e));
		return 1;
	}
	process_samples_from_track(movie, track_id);
	seg_curr++;

	/* Process segments */
	while (seg_curr <= seg_max) {
		fprintf(stdout, "Process segment %5d/%5d: %s\n", seg_curr, seg_max, argv[seg_curr+1]);

		/* Open the segment */
		e = gf_isom_open_segment(movie, argv[seg_curr+1], 0, 0, GF_FALSE);
		if (e != GF_OK) {
			fprintf(stdout, "Could not open segment %s for reading (%s).\n", argv[seg_curr+1], gf_error_to_string(e));
			ret = 1;
			goto exit;
		}

		/* Process the segment */
		process_samples_from_track(movie, track_id);

		/* Release the segment */
		gf_isom_release_segment(movie, 0);

		seg_curr++;
	}

exit:
	gf_isom_release_segment(movie, 1);
	gf_isom_close(movie);
#if defined(DEBUG) || defined(_DEBUG)
	/* Closes GPAC memory tracking in debug mode only */
	gf_sys_close();
#endif

	return ret;
}
