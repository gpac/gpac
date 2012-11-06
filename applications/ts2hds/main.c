/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Author: Romain Bouqueau
 *			Copyright (c) Romain Bouqueau 2012-
 *				All rights reserved
 *
 *          Note: this development was kindly sponsorized by Vizion'R (http://vizionr.com)
 *
 *  This file is part of GPAC / TS to HDS (ts2hds) application
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

#include "ts2hds.h"

#ifdef WIN32
#define strnicmp _strnicmp
#endif

#define CHECK_NEXT_ARG	if (i+1==(u32)argc) { fprintf(stderr, "Missing arg - please check usage\n"); exit(1); }

#ifdef GPAC_DISABLE_ISOM

#error "Cannot compile TS2HDS if GPAC is not built with ISO File Format support"

#endif


static GFINLINE void usage(const char * progname) 
{
	fprintf(stderr, "USAGE: %s -i input -o output\n"
					"\n"
#ifdef GPAC_MEMORY_TRACKING
					"\t-mem-track:  enables memory tracker\n"
#endif
		);
}


/*parse TS2HDS arguments*/
static GFINLINE GF_Err parse_args(int argc, char **argv, char **input, char **output, u64 *curr_time)
{
	Bool input_found=0, output_found=0;
	char *arg = NULL, *error_msg = "no argument found";
	s32 i;

	for (i=1; i<argc; i++) {
		arg = argv[i];
		if (!strnicmp(arg, "-h", 2) || strstr(arg, "-help")) {
			usage(argv[0]);
			return GF_EOS;
		} else if (!strnicmp(arg, "-input", 6)) {
			CHECK_NEXT_ARG
			*input = argv[i+1];
			input_found = 1;
			i++;
		} else if (!strnicmp(arg, "-output", 6)) {
			CHECK_NEXT_ARG
			*output = argv[i+1];
			output_found = 1;
			i++;
		} else if (!strnicmp(arg, "-mem-track", 10)) {
#ifdef GPAC_MEMORY_TRACKING
			gf_sys_close();
			gf_sys_init(1);
			gf_log_set_tool_level(GF_LOG_MEMORY, GF_LOG_INFO);
#else
			fprintf(stderr, "WARNING - GPAC not compiled with Memory Tracker - ignoring \"-mem-track\"\n"); 
#endif
		} else {
			error_msg = "unknown option \"%s\"";
			goto error;
		}
	}

	/*syntax is correct; now testing the presence of mandatory arguments*/
	if (input_found && output_found) {
		return GF_OK;
	} else {
		if (!input_found)
			fprintf(stderr, "Error: input argument not found\n\n");
		if (!output_found)
			fprintf(stderr, "Error: output argument not found\n\n");
		return GF_BAD_PARAM;
	}

error:	
	if (!arg) {
		fprintf(stderr, "Error: %s\n\n", error_msg);
	} else {
		fprintf(stderr, "Error: %s \"%s\"\n\n", error_msg, arg);
	}
	return GF_BAD_PARAM;
}

int main(int argc, char **argv)
{
	/********************/
	/*   declarations   */
	/********************/
	char *input, *output, tmpstr[GF_MAX_PATH];
	GF_ISOFile *isom_file_in, *isom_file_out;
	GF_MediaImporter import;
	AdobeHDSCtx ctx;
	GF_Err e;
	u32 i;
	
	/*****************/
	/*   gpac init   */
	/*****************/
	gf_sys_init(0);
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
	
	/***********************/
	/*   initialisations   */
	/***********************/
	input = NULL;
	output = NULL;
	isom_file_in = NULL;
	isom_file_out = NULL;
	memset(&import, 0, sizeof(GF_MediaImporter));
	e = GF_OK;
	memset(&ctx, 0, sizeof(ctx));

	/*********************************************/
	/*   parse arguments and build HDS context   */
	/*********************************************/
	if (GF_OK != parse_args(argc, argv, &input, &output, &ctx.curr_time)) {
		usage(argv[0]);
		goto exit;
	}
	
	ctx.multirate_manifest = adobe_alloc_multirate_manifest(output);
	ctx.curr_time = 0;

#if 0 /*'abst'conversion tests*/
	{
		char bootstrap64[GF_MAX_PATH];
		u32 bootstrap64_len;
		unsigned char bootstrap[GF_MAX_PATH];
		u32 bootstrap_len=GF_MAX_PATH;
		GF_AdobeBootstrapInfoBox *abst = (GF_AdobeBootstrapInfoBox *)abst_New();
		FILE *f = fopen(input, "rt");
		GF_BitStream *bs;
		gf_f64_seek(f, 0, SEEK_END);
		bootstrap64_len = (u32)gf_f64_tell(f);
		gf_f64_seek(f, 0, SEEK_SET);
		fread(bootstrap64, bootstrap64_len, 1, f);
		bootstrap_len = gf_base64_decode(bootstrap64, bootstrap64_len, bootstrap, bootstrap_len);
		bs = gf_bs_new(bootstrap+8, bootstrap_len-8, GF_BITSTREAM_READ);
		abst->size = bootstrap[2]*16+bootstrap[3];
		abst_Read((GF_Box*)abst, bs);
		gf_bs_del(bs);
		//then rewrite with just one 'afrt'
		memset(bootstrap, 0, bootstrap_len);
		bs = gf_bs_new(bootstrap, bootstrap_len, GF_BITSTREAM_WRITE);
		abst_Write((GF_Box*)abst, bs);
		bootstrap_len = gf_bs_get_position(bs);
		gf_bs_del(bs);
		fclose(f);
		f = fopen("bootstrap.new", "wt");
		bootstrap64_len = gf_base64_encode(bootstrap, bootstrap_len, bootstrap64, GF_MAX_PATH);
		fwrite(bootstrap64, bootstrap64_len, 1, f);
		fprintf(f, "\n\n");
		abst_dump((GF_Box*)abst, f);
		fclose(f);
		abst_del((GF_Box*)abst);
		return 0;
	}
#endif

	/*****************/
	/*   main loop   */
	/*****************/
	import.trackID = 0;
	import.in_name = input;
	import.flags = GF_IMPORT_PROBE_ONLY;

	//create output or open when recovering from a saved state
	strcpy(tmpstr, input);
	strcat(tmpstr, "_import.mp4");
	isom_file_in = gf_isom_open(tmpstr, GF_ISOM_WRITE_EDIT, NULL);
	if (!isom_file_in) {
		fprintf(stderr, "Error opening output file %s: %s\n", tmpstr, gf_error_to_string(e));
		assert(0);
		goto exit;
	}
	import.dest = isom_file_in;

	//probe input
	e = gf_media_import(&import);
	if (e) {
		fprintf(stderr, "Error while importing input file %s: %s\n", input, gf_error_to_string(e));
		assert(0);
		goto exit;
	}

	//import input data
	import.flags = 0;
	for (i=0; i<import.nb_tracks; i++) {
		import.trackID = import.tk_info[i].track_num;
		e = gf_media_import(&import);
		if (e) {
			assert(0);
			goto exit;
		}
	}

	//Adobe specific stuff
	e = adobize_segment(isom_file_in, &ctx);
	if (e) {
		fprintf(stderr, "Couldn't turn the ISOM fragmented file into an Adobe f4v segment: %s\n", gf_error_to_string(e));
		assert(0);
		goto exit;
	}

	e = adobe_gen_multirate_manifest(ctx.multirate_manifest, ctx.bootstrap, ctx.bootstrap_size);
	if (e) {
		fprintf(stderr, "Couldn't generate Adobe f4m manifest: %s\n", gf_error_to_string(e));
		assert(0);
		goto exit;
	}

	//interleave data
	//FIXME: set multiple fragments: e = gf_media_fragment_file(isom_file_in, output, 1.0);
	e = gf_media_fragment_file(isom_file_in, "test_HD_100Seg1-Frag1"/*output*/, 1.0+gf_isom_get_duration(isom_file_in)/gf_isom_get_timescale(isom_file_in));
	if (e) {
		fprintf(stderr, "Error while fragmenting file to output %s: %s\n", output, gf_error_to_string(e));
		assert(0);
		goto exit;
	}
	gf_isom_delete(isom_file_in);
	isom_file_in = NULL;
#if 0
	isom_file_out = gf_isom_open(output, GF_ISOM_OPEN_EDIT, NULL); //FIXME: check in gf_media_fragment_file() we really need this
	if (!isom_file_out) {
		fprintf(stderr, "Error opening output file %s: %s\n", output, gf_error_to_string(e));
		assert(0);
		goto exit;
	}

	//Adobe specific stuff
	e = adobize_segment(isom_file_out, &ctx);
	if (e) {
		fprintf(stderr, "Couldn't turn the ISOM fragmented file into an Adobe f4v segment: %s\n", gf_error_to_string(e));
		assert(0);
		goto exit;
	}

	e = adobe_gen_multirate_manifest(ctx.multirate_manifest, ctx.bootstrap, ctx.bootstrap_size);
	if (e) {
		fprintf(stderr, "Couldn't generate Adobe f4m manifest: %s\n", gf_error_to_string(e));
		assert(0);
		goto exit;
	}
#endif

exit:
	//delete intermediate mp4 file
	if (isom_file_in)
		gf_isom_delete(isom_file_in);

#if 0
	if (isom_file_out) {
		//FIXME: GPAC does not output to the required filename (adds "out_" prefix).
		char fname[GF_MAX_PATH];
		fname[0] = 0;
		strcat(fname, "out_");
		strcat(fname, gf_isom_get_filename(isom_file_out));
		e = gf_isom_close(isom_file_out);
		assert(!e);
		e = gf_delete_file(output);
		assert(!e);
		e = gf_move_file(fname, "test_HD_100Seg1-Frag1"/*output*/);
		if (!e) {
			fprintf(stderr, "Warning: couldn't copy output file %s: %s\n", output, gf_error_to_string(e));
			e = GF_OK;
		}
	}
#endif

	if (ctx.multirate_manifest)
		adobe_free_multirate_manifest(ctx.multirate_manifest);

	if (ctx.bootstrap) {
		gf_free(ctx.bootstrap);
		//ctx.bootstrap = NULL;
		//ctx.bootstrap_size = 0;
	}

	gf_sys_close();
	
	return !e ? 0 : 1;
}

