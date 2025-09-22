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

//FIXME: test only
//#include <gpac/internal/isomedia_dev.h>
//#include <gpac/base_coding.h>

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
	        , progname);
}


/*parse TS2HDS arguments*/
static GFINLINE GF_Err parse_args(int argc, char **argv, char **input, char **output, u64 *curr_time, u32 *segnum)
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
			*input = argv[++i];
			input_found = 1;
		} else if (!strnicmp(arg, "-output", 7)) {
			CHECK_NEXT_ARG
			*output = argv[++i];
			output_found = 1;
		} else if (!strnicmp(arg, "-segnum", 7)) {
			CHECK_NEXT_ARG
			*segnum = atoi(argv[++i]);
		} else if (!strnicmp(arg, "-mem-track", 10)) {
#ifdef GPAC_MEMORY_TRACKING
			gf_sys_close();
			gf_sys_init(GF_MemTrackerSimple);
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
	GF_ISOFile *isom_file_in;
	GF_MediaImporter import;
	AdobeHDSCtx ctx;
	GF_Err e;
	u32 i;

	/*****************/
	/*   gpac init   */
	/*****************/
	gf_sys_init(GF_MemTrackerNone);
	gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);

	/***********************/
	/*   initialisations   */
	/***********************/
	input = NULL;
	output = NULL;
	isom_file_in = NULL;
	memset(&import, 0, sizeof(GF_MediaImporter));
	e = GF_OK;
	memset(&ctx, 0, sizeof(ctx));

	ctx.curr_time = 0;
	ctx.segnum = 1;

	/*********************************************/
	/*   parse arguments and build HDS context   */
	/*********************************************/
	if (GF_OK != parse_args(argc, argv, &input, &output, &ctx.curr_time, &ctx.segnum)) {
		usage(argv[0]);
		goto exit;
	}

	ctx.multirate_manifest = adobe_alloc_multirate_manifest(output);

#if 0 /*'moov' conversion tests*/
	{
		char metamoov64[GF_MAX_PATH];
		u32 metamoov64_len;
		unsigned char metamoov[GF_MAX_PATH];
		u32 metamoov_len=GF_MAX_PATH;
		FILE *f = gf_fopen("metamoov64"/*input*/, "rt");
		gf_fseek(f, 0, SEEK_END);
		metamoov64_len = (u32)gf_ftell(f);
		gf_fseek(f, 0, SEEK_SET);
		fread(metamoov64, metamoov64_len, 1, f);
		metamoov_len = gf_base64_decode(metamoov64, metamoov64_len, metamoov, metamoov_len);
		gf_fclose(f);
		f = gf_fopen("metamoov", "wb");
		fwrite(metamoov, metamoov_len, 1, f);
		gf_fclose(f);
		return 0;
	}
#endif

#if 0 /*'abst'conversion tests*/
	{
		char bootstrap64[GF_MAX_PATH];
		u32 bootstrap64_len;
		unsigned char bootstrap[GF_MAX_PATH];
		u32 bootstrap_len=GF_MAX_PATH;
		GF_AdobeBootstrapInfoBox *abst = (GF_AdobeBootstrapInfoBox *)abst_New();
		GF_BitStream *bs;
#if 1 //64
		FILE *f = gf_fopen("bootstrap64"/*input*/, "rt");
		gf_fseek(f, 0, SEEK_END);
		bootstrap64_len = (u32)gf_ftell(f);
		gf_fseek(f, 0, SEEK_SET);
		fread(bootstrap64, bootstrap64_len, 1, f);
		bootstrap_len = gf_base64_decode(bootstrap64, bootstrap64_len, bootstrap, bootstrap_len);
#else //binary bootstrap
		FILE *f = gf_fopen("bootstrap.bin"/*input*/, "rb");
		gf_fseek(f, 0, SEEK_END);
		bootstrap_len = (u32)gf_ftell(f);
		gf_fseek(f, 0, SEEK_SET);
		fread(bootstrap, bootstrap_len, 1, f);
#endif
		bs = gf_bs_new(bootstrap+8, bootstrap_len-8, GF_BITSTREAM_READ);
		abst->size = bootstrap[2]*256+bootstrap[3];
		assert(abst->size<GF_MAX_PATH);
		abst_Read((GF_Box*)abst, bs);
		gf_bs_del(bs);
		//then rewrite with just one 'afrt'
		memset(bootstrap, 0, bootstrap_len);
		bs = gf_bs_new(bootstrap, bootstrap_len, GF_BITSTREAM_WRITE);
		abst_Write((GF_Box*)abst, bs);
		bootstrap_len = (u32)gf_bs_get_position(bs);
		gf_bs_del(bs);
		gf_fclose(f);
		f = gf_fopen("bootstrap", "wt");
		bootstrap64_len = gf_base64_encode(bootstrap, bootstrap_len, bootstrap64, GF_MAX_PATH);
		fwrite(bootstrap64, bootstrap64_len, 1, f);
		fprintf(f, "\n\n");
		abst_dump((GF_Box*)abst, f);
		gf_fclose(f);
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
	sprintf(tmpstr, "%s_import.mp4", input);
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
			fprintf(stderr, "Error while importing track number %u, input file %s: %s\n", import.trackID, input, gf_error_to_string(e));
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

	//interleave data and remove imported file
	//FIXME: set multiple fragments:
	sprintf(tmpstr, "%s_HD_100_Seg%u-Frag1", output, ctx.segnum); //FIXME: "HD", "100" and fragnum: pass as arg
	e = gf_media_fragment_file(isom_file_in, tmpstr, 1.0+gf_isom_get_duration(isom_file_in)/gf_isom_get_timescale(isom_file_in), GF_FALSE);
	if (e) {
		fprintf(stderr, "Error while fragmenting file to output %s: %s\n", output, gf_error_to_string(e));
		assert(0);
		goto exit;
	}
	gf_isom_delete(isom_file_in);
	isom_file_in = NULL;

	e = adobe_gen_multirate_manifest(ctx.multirate_manifest, ctx.bootstrap, ctx.bootstrap_size);
	if (e) {
		fprintf(stderr, "Couldn't generate Adobe f4m manifest: %s\n", gf_error_to_string(e));
		assert(0);
		goto exit;
	}

exit:
	//delete intermediate mp4 file
	if (isom_file_in)
		gf_isom_delete(isom_file_in);

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

