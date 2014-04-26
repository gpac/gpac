/*
*			GPAC - Multimedia Framework C SDK
*
 *			Authors: Cyril COncolato, Romain Bouqueau
 *			Copyright (c) Telecom ParisTech 2008-2012
*					All rights reserved
*
*  This file is part of GPAC / udp TS segmenter (udptsseg) application
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
#include <gpac/base_coding.h>
#include <gpac/ietf.h>
#include <gpac/mpegts.h>

#define UDP_BUFFER_SIZE	64484

/* adapted from http://svn.assembla.com/svn/legend/segmenter/segmenter.c */
static GF_Err write_manifest(char *manifest, char *segment_dir, u32 segment_duration, char *segment_prefix, char *http_prefix,
                             u32 first_segment, u32 last_segment, Bool end) {
	FILE *manifest_fp;
	u32 i;
	char manifest_tmp_name[GF_MAX_PATH];
	char manifest_name[GF_MAX_PATH];
	char *tmp_manifest = manifest_tmp_name;

	if (segment_dir) {
		sprintf(manifest_tmp_name, "%stmp.m3u8", segment_dir);
		sprintf(manifest_name, "%s%s", segment_dir, manifest);
	} else {
		sprintf(manifest_tmp_name, "tmp.m3u8");
		sprintf(manifest_name, "%s", manifest);
	}

	manifest_fp = fopen(tmp_manifest, "w");
	if (!manifest_fp) {
		fprintf(stderr, "Could not create m3u8 manifest file (%s)\n", tmp_manifest);
		return GF_BAD_PARAM;
	}

	fprintf(manifest_fp, "#EXTM3U\n#EXT-X-TARGETDURATION:%u\n#EXT-X-MEDIA-SEQUENCE:%u\n", segment_duration, first_segment);

	for (i = first_segment; i <= last_segment; i++) {
		fprintf(manifest_fp, "#EXTINF:%u,\n%s%s_%u.ts\n", segment_duration, http_prefix, segment_prefix, i);
	}

	if (end) {
		fprintf(manifest_fp, "#EXT-X-ENDLIST\n");
	}
	fclose(manifest_fp);

	if (!rename(tmp_manifest, manifest_name)) {
		return GF_OK;
	} else {
		if (remove(manifest_name)) {
			fprintf(stdout, "Error removing file %s\n", manifest_name);
			return GF_IO_ERR;
		} else if (rename(tmp_manifest, manifest_name)) {
			fprintf(stderr, "Could not rename temporary m3u8 manifest file (%s) into %s\n", tmp_manifest, manifest_name);
			return GF_IO_ERR;
		} else {
			return GF_OK;
		}
	}
}

void usage()
{
	fprintf(stderr, "usage: udptsseg -src=UDP -dst-file=FILE -segment-duration=DUR -segment-dir=DIR -segment-manifest=M3U8 -segment-http-prefix=P -segment-number=N\n"
	        "-src=UDP                udp://address:port providing the input transport stream\n"
	        "-dst-file=FILE          e.g. out.ts, radical name of all segments\n"
	        "-segment-dir=DIR        server local directory to store segments (with the trailing path separator)\n"
	        "-segment-duration=DUR   segment duration in seconds\n"
	        "-segment-manifest=M3U8  m3u8 file basename\n"
	        "-segment-http-prefix=P  client address for accessing server segments\n"
	        "-segment-number=N       only n segments are used using a cyclic pattern\n"
	        "\n");
}

int main(int argc, char **argv)
{
	u32 i;
	char *arg = NULL;
	char *ts_in = NULL;
	char *segment_dir = NULL;
	char *segment_manifest = NULL;
	char *segment_http_prefix = NULL;
	u32 run_time = 0;
	char *input_ip = NULL;
	u32 input_port = 0;
	GF_Socket *input_udp_sk = NULL;
	char *input_buffer = NULL;
	u32 input_buffer_size = UDP_BUFFER_SIZE;
	GF_Err e = GF_OK;
	FILE *ts_output_file = NULL;
	char *ts_out = NULL;
	char segment_prefix[GF_MAX_PATH];
	char segment_name[GF_MAX_PATH];
	u32 segment_duration = 0;
	u32 segment_index = 0;
	u32 segment_number = 0;
	char segment_manifest_default[GF_MAX_PATH];
	u32 run = 1;
	u32 last_segment_time = 0;
	u32 last_segment_size = 0;
	u32 read = 0;
	u32 towrite = 0;
	u32 leftinbuffer = 0;

	fprintf(stdout, "UDP Transport Stream Segmenter\n");

	if (argc < 7) {
		usage();
		return 0;
	}
	/*****************/
	/*   gpac init   */
	/*****************/
	gf_sys_init(0);

	/*****************/
	/*   parsing of the arguments */
	/*****************/
	for (i = 1; i < (u32) argc ; i++) {
		arg = argv[i];
		if (!strnicmp(arg, "-src=udp://",11)) {
			char *sep;
			arg+=11;
			sep = strchr(arg+6, ':');
			if (sep) {
				input_port = atoi(sep+1);
				sep[0]=0;
				input_ip = gf_strdup(arg);
				sep[0]=':';
			} else {
				input_ip = gf_strdup(arg);
			}

		} else if (!strnicmp(arg, "-time=", 6)) {
			run_time = atoi(arg+6);
		} else if (!strnicmp(arg, "-dst-file=", 10)) {
			ts_out = gf_strdup(arg+10);
		} else if (!strnicmp(arg, "-segment-dir=", 13)) {
			segment_dir = gf_strdup(arg+13);
		} else if (!strnicmp(arg, "-segment-duration=", 18)) {
			segment_duration = atoi(arg+18);
		} else if (!strnicmp(arg, "-segment-manifest=", 18)) {
			segment_manifest = gf_strdup(arg+18);
		} else if (!strnicmp(arg, "-segment-http-prefix=", 21)) {
			segment_http_prefix = gf_strdup(arg+21);
		} else if (!strnicmp(arg, "-segment-number=", 16)) {
			segment_number = atoi(arg+16);
		}
	}
	fprintf(stdout, "Listening to TS input on %s:%d\n", input_ip, input_port);
	fprintf(stdout, "Creating %d sec. segments in directory %s\n", segment_duration, segment_dir);
	fprintf(stdout, "Creating %s manifest with %d segments\n", segment_manifest, segment_number);

	/*****************/
	/*   creation of the input socket   */
	/*****************/
	input_udp_sk = gf_sk_new(GF_SOCK_TYPE_UDP);
	if (gf_sk_is_multicast_address((char *)input_ip)) {
		e = gf_sk_setup_multicast(input_udp_sk, (char *)input_ip, input_port, 32, 0, NULL);
	} else {
		e = gf_sk_bind(input_udp_sk, NULL, input_port, (char *)input_ip, input_port, GF_SOCK_REUSE_PORT);
	}
	if (e) {
		fprintf(stdout, "Error initializing UDP socket for %s:%d : %s\n", input_ip, input_port, gf_error_to_string(e));
		goto exit;
	}
	gf_sk_set_buffer_size(input_udp_sk, 0, UDP_BUFFER_SIZE);
	gf_sk_set_block_mode(input_udp_sk, 1);

	/*****************/
	/*   Initialisation of the TS and Manifest files   */
	/*****************/

	if (segment_duration) {
		char *dot;
		strcpy(segment_prefix, ts_out);
		dot = strrchr(segment_prefix, '.');
		dot[0] = 0;
		if (segment_dir) {
			if (strchr("\\/", segment_name[strlen(segment_name)-1])) {
				sprintf(segment_name, "%s%s_%d.ts", segment_dir, segment_prefix, segment_index);
			} else {
				sprintf(segment_name, "%s%c%s_%d.ts", segment_dir, GF_PATH_SEPARATOR, segment_prefix, segment_index);
			}
		} else {
			sprintf(segment_name, "%s_%d.ts", segment_prefix, segment_index);
		}
		fprintf(stderr, "Processing %s segment\r", segment_name);
		ts_out = gf_strdup(segment_name);
		if (!segment_manifest) {
			sprintf(segment_manifest_default, "%s.m3u8", segment_prefix);
			segment_manifest = segment_manifest_default;
		}
		//write_manifest(segment_manifest, segment_dir, segment_duration, segment_prefix, segment_http_prefix, segment_index, 0, 0);
	}
	ts_output_file = fopen(ts_out, "wb");
	if (!ts_output_file) {
		fprintf(stderr, "Error opening %s\n", ts_out);
		goto exit;
	}

	/*allocate data buffer*/
	input_buffer = (char*)gf_malloc(input_buffer_size);
	assert(input_buffer);

	/*****************/
	/*   main loop   */
	/*****************/
	last_segment_time = gf_sys_clock();
	last_segment_size = 0;
	while (run) {
		/*check for some input from the network*/
		if (input_ip) {
			gf_sk_receive(input_udp_sk, input_buffer+leftinbuffer, input_buffer_size-leftinbuffer, 0, &read);
			leftinbuffer += read;
			if (leftinbuffer) {
				fprintf(stderr, "Processing %s segment ... received %d bytes (buffer: %d, segment: %d)\n", segment_name, read, leftinbuffer, last_segment_size);
				if (input_buffer[0] != 0x47) {
					u32 i = 0;
					while (input_buffer[i] != 0x47 && i < leftinbuffer) i++;
					fprintf(stderr, "Warning: data in buffer not starting with the MPEG-2 TS sync byte, skipping %d bytes of %d\n", i, leftinbuffer);
					if (i < leftinbuffer) memmove(input_buffer, input_buffer+i, leftinbuffer-i);
					leftinbuffer -=i;
				}
				if ((leftinbuffer % 188) != 0) {
					fprintf(stderr, "Warning: data in buffer with a size (%d bytes) not multiple of 188 bytes\n", leftinbuffer);
					towrite = leftinbuffer - (leftinbuffer % 188);
				} else {
					towrite = leftinbuffer;
				}
			} else {
				towrite = 0;
			}
			/*write to current file */
			if (ts_output_file != NULL) {
				u32 now = gf_sys_clock();
				if (towrite) {
					gf_fwrite(input_buffer, 1, towrite, ts_output_file);
					if (towrite < leftinbuffer) {
						fprintf(stderr, "Warning: wrote %d bytes, keeping %d bytes\n", towrite, (leftinbuffer-towrite));
						memmove(input_buffer, input_buffer+towrite, leftinbuffer-towrite);
					}
					leftinbuffer -= towrite;
					last_segment_size += towrite;
				}
				if ((now - last_segment_time) > segment_duration*1000) {
					last_segment_time = now;
					fclose(ts_output_file);
					fprintf(stderr, "Closing segment %s (%d bytes)\n", segment_name, last_segment_size);
					last_segment_size = 0;
					segment_index++;
					if (segment_dir) {
						if (strchr("\\/", segment_name[strlen(segment_name)-1])) {
							sprintf(segment_name, "%s%s_%d.ts", segment_dir, segment_prefix, segment_index);
						} else {
							sprintf(segment_name, "%s%c%s_%d.ts", segment_dir, GF_PATH_SEPARATOR, segment_prefix, segment_index);
						}
					} else {
						sprintf(segment_name, "%s_%d.ts", segment_prefix, segment_index);
					}
					ts_output_file = fopen(segment_name, "wb");
					if (!ts_output_file) {
						fprintf(stderr, "Error opening segment %s\n", segment_name);
						goto exit;
					}
					/* delete the oldest segment */
					if (segment_number && ((s32) (segment_index - segment_number - 1) >= 0)) {
						char old_segment_name[GF_MAX_PATH];
						if (segment_dir) {
							if (strchr("\\/", segment_name[strlen(segment_name)-1])) {
								sprintf(old_segment_name, "%s%s_%d.ts", segment_dir, segment_prefix, segment_index - segment_number - 1);
							} else {
								sprintf(old_segment_name, "%s/%s_%d.ts", segment_dir, segment_prefix, segment_index - segment_number - 1);
							}
						} else {
							sprintf(old_segment_name, "%s_%d.ts", segment_prefix, segment_index - segment_number - 1);
						}
						gf_delete_file(old_segment_name);
						fprintf(stderr, "Deleting segment %s\n", old_segment_name);
					}
					write_manifest(segment_manifest, segment_dir, segment_duration, segment_prefix, segment_http_prefix,
					               //								   (segment_index >= segment_number/2 ? segment_index - segment_number/2 : 0), segment_index >1 ? segment_index-1 : 0, 0);
					               ( (segment_index > segment_number ) ? segment_index - segment_number : 0), segment_index >1 ? segment_index-1 : 0, 0);
				}
			}

			//}
		}
		/*cpu load regulation*/
		gf_sleep(1);
	}
exit:
	return 0;
}
