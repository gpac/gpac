/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Cyril Concolato 2000-2006
 *					All rights reserved
 *
 *  This file is part of GPAC / load&compare application
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
#include <gpac/scene_manager.h>
#include <zlib.h>

typedef struct {
	FILE *out;
	u32 nbloads;
	u32 verbose;
	char tmpdir[100];
} GF_LoadCompare;

u32 getlasertracksize(char *in)
{
	u32 j, totalsize = 0;
	u32 track_id, trackNum;
	
	GF_ISOFile *mp4 = gf_isom_open(in, GF_ISOM_OPEN_READ, NULL);
	if (!mp4) return 0;
	
	track_id = gf_isom_get_track_id(mp4, 1);
	trackNum = gf_isom_get_track_by_id(mp4, track_id);
	for (j=0; j<gf_isom_get_sample_count(mp4, trackNum); j++) {
		GF_ISOSample *samp = gf_isom_get_sample_info(mp4, trackNum, j+1, NULL, NULL);
		totalsize += samp->dataLength;
		gf_isom_sample_del(&samp);
	}
	gf_isom_close(mp4);
	return totalsize;
}

GF_Err dumpsvg(char *in, char *out)
{
	GF_Err e;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
	GF_SceneLoader load;
	e = GF_OK;

	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = in;
	load.ctx = ctx;

	load.isom = gf_isom_open(in, GF_ISOM_OPEN_READ, NULL);
	if (!load.isom) return GF_ISOM_INVALID_FILE;

	e = gf_sm_load_init(&load);
	if (e) {
		fprintf(stderr, "Error loading SVG file\n");
		goto err_exit;
	}
	
	e = gf_sm_load_run(&load);
	if (e) {
		fprintf(stderr, "Error loading SVG file\n");
		goto err_exit;
	}

	e = gf_sm_dump(ctx, out, GF_SM_DUMP_SVG);

err_exit:
	gf_sm_load_done(&load);
	gf_sm_del(ctx);
	gf_sg_del(sg);
	gf_isom_close(load.isom);
	return e;
}

GF_Err encodelaser(GF_LoadCompare *lc, char *in, GF_ISOFile *mp4, GF_SMEncodeOptions *opts) 
{
	GF_Err e;
	GF_SceneLoader load;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
	GF_StatManager *statsman = NULL;
	
	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	memset(&load, 0, sizeof(GF_SceneLoader));
	load.fileName = in;
	load.ctx = ctx;
	e = gf_sm_load_init(&load);
	e = gf_sm_load_run(&load);
	gf_sm_load_done(&load);

	if (opts->auto_qant) {
		if (lc->verbose) fprintf(stdout, "Analysing Scene for Automatic Quantization\n");
		statsman = gf_sm_stats_new();
		e = gf_sm_stats_for_scene(statsman, ctx);
		if (!e) {
			GF_SceneStatistics *stats = gf_sm_stats_get(statsman);
			if (opts->resolution > (s32)stats->frac_res_2d) {
				if (lc->verbose) fprintf(stdout, " Given resolution %d is (unnecessarily) too high, using %d instead.\n", opts->resolution, stats->frac_res_2d);
				opts->resolution = stats->frac_res_2d;
			} else if (stats->int_res_2d + opts->resolution <= 0) {
				if (lc->verbose) fprintf(stdout, " Given resolution %d is too low, using %d instead.\n", opts->resolution, stats->int_res_2d - 1);
				opts->resolution = 1 - stats->int_res_2d;
			}				
			opts->coord_bits = stats->int_res_2d + opts->resolution;
			if (lc->verbose) fprintf(stdout, " Coordinates & Lengths encoded using ");
			if (opts->resolution < 0) {
				if (lc->verbose) fprintf(stdout, "only the %d most significant bits (of %d).\n", opts->coord_bits, stats->int_res_2d);
			} else {
				if (lc->verbose) fprintf(stdout, "a %d.%d representation\n", stats->int_res_2d, opts->resolution);
			}

			if (lc->verbose) fprintf(stdout, " Matrix Scale & Skew Coefficients ");
			if (opts->coord_bits < stats->scale_int_res_2d) {
				opts->scale_bits = stats->scale_int_res_2d - opts->coord_bits;
				if (lc->verbose) fprintf(stdout, "encoded using a %d.8 representation\n", stats->scale_int_res_2d);
			} else  {
				opts->scale_bits = 0;
				if (lc->verbose) fprintf(stdout, "not encoded.\n");
			}
		}
	}

	if (e) {
		fprintf(stderr, "Error loading file %s\n", gf_error_to_string(e));
		goto err_exit;
	} else {
		e = gf_sm_encode_to_file(ctx, mp4, opts);
	}

	gf_isom_set_brand_info(mp4, GF_ISOM_BRAND_MP42, 1);
	gf_isom_modify_alternate_brand(mp4, GF_ISOM_BRAND_ISOM, 1);

err_exit:
	if (statsman) gf_sm_stats_del(statsman);
	gf_sm_del(ctx);
	gf_sg_del(sg);
	return e;
}

u32 loadonefile(char *item_name, Bool is_mp4, u32 nbloads) 
{
	GF_Err e;
	u32 starttime,endtime, totaltime, i; 
	GF_SceneGraph *sg;
	GF_SceneLoader load;

	totaltime = 0;
	
	for (i =0; i<nbloads; i++) {
		memset(&load, 0, sizeof(GF_SceneLoader));
		sg = gf_sg_new();
		load.ctx = gf_sm_new(sg);
		load.fileName = item_name;
		if (is_mp4) load.isom = gf_isom_open(item_name, GF_ISOM_OPEN_READ, NULL);
		starttime = gf_sys_clock();

		e = gf_sm_load_init(&load);
		if (e) {
			fprintf(stderr, "Error loading file %s\n", item_name);
			goto err_exit;
		}

		e = gf_sm_load_run(&load);
		if (e) {
			fprintf(stderr, "Error loading file %s\n", item_name);
			goto err_exit;
		}

		endtime = gf_sys_clock();
		
		gf_sm_load_done(&load);
		gf_sm_del(load.ctx);
		gf_sg_del(sg);
		if (is_mp4) gf_isom_close(load.isom);

		totaltime += endtime-starttime;
	}

	return totaltime;
err_exit:
	gf_sm_load_done(&load);
	gf_sm_del(load.ctx);
	gf_sg_del(sg);
	if (is_mp4) gf_isom_close(load.isom);
	return 0;
}

Bool loadcompare_one(void *cbck, char *item_name, char *item_path)
{
	GF_Err e = GF_OK;
	u32 lasersize;
	FILE *tmpfile;
	char name[100], name2[100];
	char *tmp;
	GF_LoadCompare *lc = cbck;
	u32 time;

	strcpy(name, (const char*)item_name);
	tmp = strrchr(name, '.');
	tmp[0] = 0;
	if (lc->verbose) fprintf(stdout,"Processing %s\n", name);
	fprintf(lc->out,"%s", name);
	tmp[0] = '.';

	/* MP4 */
	if (lc->verbose) fprintf(stdout,"Looking for MP4 file ... ");
	strcpy(name, (const char*)item_name);
	tmp = strrchr(name, '.');
	strcpy(tmp, ".mp4");
	tmpfile = fopen(name, "rb");
	if (!tmpfile) { // encoding the file if the MP4 does not exist
		GF_SMEncodeOptions opts;
		GF_ISOFile *mp4;
		if (lc->verbose) fprintf(stdout,"not present.\nEncoding SVG into LASeR... \n");

		memset(&opts, 0, sizeof(GF_SMEncodeOptions));
		opts.auto_qant = 1;
		opts.resolution = 8;
		mp4 = gf_isom_open(name, GF_ISOM_OPEN_WRITE, NULL);
		if (mp4) {
			e = encodelaser(lc, item_name, mp4, &opts);
			if (e) {
				fprintf(stderr, "Error in LASeR encoding %s\n", item_name);
				return 1;
			}
			gf_isom_close(mp4);
		} else {
			fprintf(stderr, "Error: MP4 file could not be created\n");
			return 1;
		}
	} else {
		if (lc->verbose) fprintf(stdout,"present.\n");
		fclose(tmpfile);
	}
	
	if (lc->verbose) fprintf(stdout,"Loading and decoding %d time(s) the MP4 file ... \n", lc->nbloads);
	time = loadonefile(name, 1, lc->nbloads);
	if (time) fprintf(lc->out,"\t%d", time);
	else return 1;

	/* Dump the decoded SVG */
	if (lc->verbose) fprintf(stdout,"Looking for the decoded SVG ... ");
	strcpy(name2, lc->tmpdir);
	strcat(name2, (const char*)item_name);
	tmp = strrchr(name2, '.');
	strcpy(tmp, "_out.svg");
	tmpfile = fopen(name2, "rt");
	if (!tmpfile) {
		if (lc->verbose) fprintf(stdout,"not present.\nDecoding MP4 and dumping SVG ... \n");
		tmp = strrchr(name2, '.');
		tmp[0] = 0;
		e = dumpsvg(name, name2);
		if (e) {
			fprintf(stderr, "Error dumping SVG %s\n", name2);
			return 1;
		}
		tmp[0] = '.';
	} else {
		if (lc->verbose) fprintf(stdout,"present.\n");
		fclose(tmpfile);
	}
	
	/* SVG Parsing */
	if (lc->verbose) fprintf(stdout,"Loading and parsing %d time(s) the input SVG file ... \n", lc->nbloads);
	time = loadonefile(item_name, 0, lc->nbloads);
	if (time) fprintf(lc->out,"\t%d", time);
	else return 1;
	if (lc->verbose) fprintf(stdout,"Loading and parsing %d time(s) the decoded SVG file ... \n", lc->nbloads);
	time = loadonefile(name2, 0, lc->nbloads);
	if (time) fprintf(lc->out,"\t%d", time);
	else return 1;

	/* SVGZ */
	if (lc->verbose) fprintf(stdout,"Looking for the SVGZ ... ");
	strcpy(name, (const char*)item_name);
	strcat(name, "z");
	tmpfile = fopen(name, "rb");
	if (!tmpfile) { // compress the file if the SVGZ does not exist
		char buffer[100];
		u32 size;
		void *gzFile;
		if (lc->verbose) fprintf(stdout,"not present.\nGzipping SVG ... \n");
		gzFile = gzopen(name, "wb");
		tmpfile = fopen(item_name, "rt");
		while ((size = fread(buffer, 1, 100, tmpfile))) gzwrite(gzFile, buffer, size);
		fclose(tmpfile);
		gzclose(gzFile);
	} else {
		if (lc->verbose) fprintf(stdout,"present.\n");
		fclose(tmpfile);
	}
	if (lc->verbose) fprintf(stdout,"Loading, decompressing and parsing %d time(s) the SVGZ file ... \n", lc->nbloads);
	time = loadonefile(name, 0, lc->nbloads);
	if (time) fprintf(lc->out,"\t%d", time);
	else return 1;

	/*Sizes */
	if (lc->verbose) fprintf(stdout,"Checking file sizes\n");
	tmpfile = fopen(item_name, "rt");
	if (tmpfile) { 
		fseek(tmpfile, 0, SEEK_END);
		fprintf(lc->out,"\t%d", (u32)ftell(tmpfile));
		if (lc->verbose) fprintf(stdout,"Input SVG: %d\n", (u32)ftell(tmpfile));
		fclose(tmpfile);
	} else {
		fprintf(stderr, "Cannot probe size for %s\n", item_name);
		return 1;
	}

	strcpy(name2, lc->tmpdir);
	strcat(name2, (const char*)item_name);
	tmp = strrchr(name2, '.');
	strcpy(tmp, "_out.svg");
	tmpfile = fopen(name2, "rt");
	if (tmpfile) { 
		fseek(tmpfile, 0, SEEK_END);
		fprintf(lc->out,"\t%d", (u32)ftell(tmpfile));
		if (lc->verbose) fprintf(stdout,"Output SVG: %d\n", (u32)ftell(tmpfile));
		fclose(tmpfile);
		gf_delete_file(name2);
	} else {
		fprintf(stderr, "Cannot probe size for %s\n", name2);
		return 1;
	}
	strcpy(name, (const char*)item_name);
	strcat(name, "z");
	tmpfile = fopen(name, "rb");
	fseek(tmpfile, 0, SEEK_END);
	if (tmpfile) { 
		fprintf(lc->out,"\t%d", (u32)ftell(tmpfile));
		if (lc->verbose) fprintf(stdout,"SVGZ: %d\n", (u32)ftell(tmpfile));
		fclose(tmpfile);
	//	gf_delete_file(name);
	} else {
		fprintf(stderr, "Cannot probe size for %s\n", name);
		return 1;
	}

	strcpy(name, (const char*)item_name);
	tmp = strrchr(name, '.');
	strcpy(tmp, ".mp4");
	lasersize = getlasertracksize(name);
	if (lasersize != 0) { 
		fprintf(lc->out,"\t%d", lasersize);
		if (lc->verbose) fprintf(stdout,"LASeR: %d\n", lasersize);
	//	gf_delete_file(name);
	} else {
		fprintf(stderr, "Cannot probe size for %s\n", name);
		return 1;
	}
	
	if (lc->verbose) fprintf(stdout,"%s done\n", item_name);
	fprintf(lc->out,"\n");
	fflush(lc->out);

	return 0;
}

void usage() 
{
	fprintf(stdout, "Compare LASeR and SVG encoding size and loading time\n");
	fprintf(stdout, "usage: (-out output_result) (-single input.svg | -dir dir) (-nloads X) (-verbose X)\n");
	fprintf(stdout, "defaults are: stdout, dir=. and X = 1");
}

int main(int argc, char **argv)
{
	u32 i;
	char *arg;
	GF_LoadCompare lc;
	Bool single = 0;
	char *out = NULL;
	char in[256] = ".";

	fprintf(stdout, "LASeR and SVG Comparison tool\n");

	memset(&lc, 0, sizeof(GF_LoadCompare));
	lc.nbloads = 1;
	lc.out = stdout;
	
	for (i = 1; i < (u32) argc ; i++) {
		arg = argv[i];
		if (!stricmp(arg, "-out")) {
			out = argv[i+1];
			i++;
		} else if (!stricmp(arg, "-single")) {
			single = 1;
			strcpy(in, argv[i+1]);
			i++;
		} else if (!stricmp(arg, "-dir")) {
			strcpy(in, argv[i+1]);
			i++;
		} else if (!stricmp(arg, "-nloads")) {
			lc.nbloads = (u32)atoi(argv[i+1]);
			i++;
		} else if (!stricmp(arg, "-verbose")) {
			lc.verbose = (u32)atoi(argv[i+1]);
			i++;
		} else if (!stricmp(arg, "-tmpdir")) {
			strcpy(lc.tmpdir, argv[i+1]);
			i++;
		} else {
			usage();
			return -1;
		}	
	}

	gf_sys_init();
	if (out) lc.out = fopen(out, "wt");
	if (!lc.out) {
		fprintf(stderr, "Cannot open output file %s\n", out);
		return -1;
	}

	fprintf(lc.out,"File Name\tMP4 Load Time\tInput SVG Load Time\tDecoded SVG Load Time\tSVGZ Load Time\tSVG Size\tDecoded SVG Size\tSVGZ Size\tLASeR track size\n");

	if (single) {
		loadcompare_one(&lc, in, NULL);
	} else {
		gf_enum_directory(in, 0, loadcompare_one, &lc, "svg");
	}
		
	if (lc.out) fclose(lc.out);
	gf_sys_close();
	return 0;
}

