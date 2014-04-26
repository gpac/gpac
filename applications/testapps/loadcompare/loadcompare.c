/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Cyril Concolato
 *			Copyright (c) Telecom ParisTech 2006-2012
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

enum {
	SVG = 0,
	XMT = 1,
};

typedef struct {
	char filename[100];
	u32 size;
	u32 gpacxml_loadtime;
	u32 libxml_loadtime;
	u32 gz_size;
	u32 gpacxml_gz_loadtime;
	u32 libxml_gz_loadtime;
	u32 track_size;
	u32 track_loadtime;
	u32 decoded_size;
	u32 decoded_loadtime;
} LoadData;

typedef struct {
	FILE *out;
	u32 type;
	u32 nbloads;
	u32 verbose;
	Bool regenerate;
	Bool spread_repeat;
	u32 repeat_index;
	GF_List *data;
} GF_LoadCompare;

GF_Err load_mp4(GF_LoadCompare *lc, GF_ISOFile *mp4, u32 *loadtime)
{
	GF_Err e = GF_OK;
	GF_SceneLoader load;
	GF_SceneGraph *sg;
	u32 i, starttime, endtime;
	u32 nb;
	if (lc->spread_repeat) nb = 1;
	else nb = lc->nbloads ;

	*loadtime = 0;
	for (i = 0; i< nb; i++) {
		memset(&load, 0, sizeof(GF_SceneLoader));
		sg = gf_sg_new();
		load.ctx = gf_sm_new(sg);

		load.isom = mp4;
		starttime = gf_sys_clock();

		e = gf_sm_load_init(&load);
		if (e) {
			fprintf(stderr, "Error loading MP4 file\n");
		} else {
			e = gf_sm_load_run(&load);
			if (e) {
				fprintf(stderr, "Error loading MP4 file\n");
			} else {
				endtime = gf_sys_clock();
				*loadtime += endtime-starttime;
			}
			gf_sm_load_done(&load);
		}
		gf_sm_del(load.ctx);
		gf_sg_del(sg);
	}
	return e;
}

void load_progress(void *cbk, u32 done, u32 total) {
	fprintf(stdout, "%d/%d\r", done, total);
}

GF_Err gpacctx_load_file(GF_LoadCompare *lc, char *item_path, u32 *loadtime)
{
	GF_Err e = GF_OK;
	GF_SceneLoader load;
	GF_SceneGraph *sg;
	u32 i, starttime, endtime;

	u32 nb;
	if (lc->spread_repeat) nb = 1;
	else nb = lc->nbloads ;

	*loadtime = 0;

	for (i = 0; i<nb; i++) {
		memset(&load, 0, sizeof(GF_SceneLoader));
		sg = gf_sg_new();
		load.ctx = gf_sm_new(sg);
		load.OnProgress = load_progress;

		load.fileName = item_path;
		starttime = gf_sys_clock();

		e = gf_sm_load_init(&load);
		if (e) {
			fprintf(stderr, "Error loading file %s\n", item_path);
		} else {
			e = gf_sm_load_run(&load);
			if (e) {
				fprintf(stderr, "Error loading file %s\n", item_path);
			} else {
				endtime = gf_sys_clock();
				*loadtime += endtime-starttime;
			}
			gf_sm_load_done(&load);
		}
		gf_sm_del(load.ctx);
		gf_sg_del(sg);
	}
	return e;
}

GF_Err get_laser_track_size(GF_ISOFile *mp4, u32 *size)
{
	GF_Err e = GF_OK;
	u32 j;
	u32 track_id, trackNum;

	*size = 0;
	track_id = gf_isom_get_track_id(mp4, 1);
	trackNum = gf_isom_get_track_by_id(mp4, track_id);
	for (j=0; j<gf_isom_get_sample_count(mp4, trackNum); j++) {
		GF_ISOSample *samp = gf_isom_get_sample_info(mp4, trackNum, j+1, NULL, NULL);
		*size += samp->dataLength;
		gf_isom_sample_del(&samp);
	}
	return e;
}

GF_Err encode_laser(GF_LoadCompare *lc, char *item_path, GF_ISOFile *mp4, GF_SMEncodeOptions *opts)
{
	GF_Err e = GF_OK;
	GF_SceneLoader load;
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
	GF_StatManager *statsman = NULL;

	memset(&load, 0, sizeof(GF_SceneLoader));
	sg = gf_sg_new();
	ctx = gf_sm_new(sg);
	load.ctx = ctx;
	load.fileName = item_path;

	e = gf_sm_load_init(&load);
	if (e) {
		fprintf(stderr, "Error loading file %s\n", item_path);
	} else {
		e = gf_sm_load_run(&load);
		if (e) {
			fprintf(stderr, "Error loading file %s\n", item_path);
		} else {
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
				gf_sm_stats_del(statsman);
			}

			e = gf_sm_encode_to_file(ctx, mp4, opts);
			if (e) {
				fprintf(stderr, "Error while encoding mp4 file\n");
			} else {
				e = gf_isom_set_brand_info(mp4, GF_ISOM_BRAND_MP42, 1);
				if (!e) e = gf_isom_modify_alternate_brand(mp4, GF_ISOM_BRAND_ISOM, 1);
			}

			gf_sm_load_done(&load);
		}
	}
	gf_sm_del(ctx);
	gf_sg_del(sg);

	return e;
}

GF_Err create_laser_mp4(GF_LoadCompare *lc, char *item_name, char *item_path, u32 *size)
{
	char mp4_path[100], *ext;
	GF_Err e = GF_OK;
	GF_ISOFile *mp4;

	*size = 0;

	strcpy(mp4_path, item_name);
	ext = strrchr(mp4_path, '.');
	strcpy(ext, ".mp4");
	mp4 = gf_isom_open(mp4_path, GF_ISOM_WRITE_EDIT, NULL);
	if (!mp4) {
		if (lc->verbose) fprintf(stdout, "Could not open file %s for writing\n", mp4_path);
		e = GF_IO_ERR;
	} else {
		GF_SMEncodeOptions opts;
		memset(&opts, 0, sizeof(GF_SMEncodeOptions));
		opts.auto_qant = 1;
		opts.resolution = 8;
		e = encode_laser(lc, item_path, mp4, &opts);
		if (e) {
			if (lc->verbose) fprintf(stdout, "Could not encode MP4 file from %s\n", item_path);
			gf_isom_delete(mp4);
		} else {
			gf_isom_close(mp4);

			mp4 = gf_isom_open(mp4_path, GF_ISOM_OPEN_READ, NULL);
			if (!mp4) {
				if (lc->verbose) fprintf(stdout, "Could not open file %s for reading\n", mp4_path);
				e = GF_IO_ERR;
			} else {
				e = get_laser_track_size(mp4, size);
				if (e) {
					if (lc->verbose) fprintf(stdout, "Could not get MP4 file size\n");
				}
				gf_isom_close(mp4);
			}
		}
	}
	return e;
}


GF_Err get_mp4_loadtime(GF_LoadCompare *lc, char *item_name, char *item_path, u32 *loadtime)
{
	char mp4_path[100], *ext;
	GF_Err e = GF_OK;
	GF_ISOFile *mp4;

	*loadtime = 0;

	strcpy(mp4_path, item_name);
	ext = strrchr(mp4_path, '.');
	strcpy(ext, ".mp4");
	mp4 = gf_isom_open(mp4_path, GF_ISOM_OPEN_READ, NULL);
	if (!mp4) {
		if (lc->verbose) fprintf(stdout, "Could not open file %s for reading\n", mp4_path);
		e = GF_IO_ERR;
	} else {
		e = load_mp4(lc, mp4, loadtime);
		if (e) {
			if (lc->verbose) fprintf(stdout, "Could not get MP4 file load time\n");
		}
	}
	gf_isom_close(mp4);
	return e;
}

GF_Err decode_svg(GF_LoadCompare *lc, char *item_name, char *item_path, char *svg_out_path)
{
	GF_SceneManager *ctx;
	GF_SceneGraph *sg;
	GF_SceneLoader load;
	GF_ISOFile *mp4;
	GF_Err e = GF_OK;
	char mp4_path[256];
	char *ext;

	strcpy(mp4_path, item_name);
	ext = strrchr(mp4_path, '.');
	strcpy(ext, ".mp4");
	mp4 = gf_isom_open(mp4_path, GF_ISOM_OPEN_READ, NULL);
	if (!mp4) {
		if (lc->verbose) fprintf(stdout, "Could not open file %s\n", mp4_path);
		e = GF_IO_ERR;
	} else {
		sg = gf_sg_new();
		ctx = gf_sm_new(sg);
		memset(&load, 0, sizeof(GF_SceneLoader));
		load.isom = mp4;
		load.ctx = ctx;
		e = gf_sm_load_init(&load);
		if (e) {
			fprintf(stderr, "Error loading MP4 file\n");
		} else {
			e = gf_sm_load_run(&load);
			if (e) {
				fprintf(stderr, "Error loading MP4 file\n");
			} else {
				gf_sm_load_done(&load);

				ext = strrchr(svg_out_path, '.');
				ext[0] = 0;
				e = gf_sm_dump(ctx, svg_out_path, GF_SM_DUMP_SVG);
				if (e) {
					fprintf(stderr, "Error dumping SVG from MP4 file\n");
				}
			}
		}
		gf_sm_del(ctx);
		gf_sg_del(sg);
		gf_isom_close(mp4);
	}
	return e;
}

GF_Err libxml_load_svg(GF_LoadCompare *lc, char *item_path, u32 *loadtime)
{
	GF_Err e = GF_OK;
	GF_SceneGraph *sg;
	u32 i, starttime, endtime;
	void *p;

	u32 nb;
	if (lc->spread_repeat) nb = 1;
	else nb = lc->nbloads ;

	*loadtime = 0;

	for (i = 0; i<nb; i++) {
		sg = gf_sg_new();

		starttime = gf_sys_clock();

		p = DANAE_NewSVGParser(item_path, sg);
		DANAE_SVGParser_Parse(p);
		DANAE_SVGParser_Terminate();

		endtime = gf_sys_clock();
		if (lc->verbose) fprintf(stdout, "LibXML single parsing: %d\n", endtime-starttime);
		*loadtime += endtime-starttime;

		gf_sg_del(sg);
	}
	return e;
}

GF_Err get_size(GF_LoadCompare *lc, char *item_name, char *item_path, u32 *size)
{
	GF_Err e = GF_OK;
	FILE *file = NULL;

	*size = 0;

	file = fopen(item_path, "rt");
	if (!file) {
		if (lc->verbose) fprintf(stdout, "Could not open file %s\n", item_path);
		e = GF_IO_ERR;
	} else {
		fseek(file, 0, SEEK_END);
		*size = (u32)ftell(file);
		fclose(file);
		if (*size == 0) {
			if (lc->verbose) fprintf(stdout, "File %s has a size of 0\n", item_path);
			e = GF_IO_ERR;
		}
	}
	return e;
}

GF_Err get_decoded_svg_loadtime_and_size(GF_LoadCompare *lc, char *item_name, char *item_path, u32 *loadtime, u32 *size)
{
	GF_Err e = GF_OK;
	char svg_out_name[256];
	char *ext;

	strcpy(svg_out_name, item_name);
	ext = strrchr(svg_out_name, '.');
	strcpy(ext, "_out.svg");

	*size = 0;
	*loadtime = 0;

	e = decode_svg(lc, item_name, item_path, svg_out_name);
	if (!e) {
		e = get_size(lc, svg_out_name, svg_out_name, size);
		if (e) {
			return e;
		}
		e = gpacctx_load_file(lc, svg_out_name, loadtime);
	}
	return e;
}

GF_Err create_gz_file(GF_LoadCompare *lc, char *item_name, char *item_path, u32 *size)
{
	char buffer[100];
	char gz_path[256];
	GF_Err e = GF_OK;
	FILE *file = NULL;
	void *gz = NULL;
	u32 read;

	*size = 0;

	strcpy(gz_path, item_name);
	strcat(gz_path, "z");
	gz = gzopen(gz_path, "wb");
	file = fopen(item_path, "rt");

	if (!gz || !file) {
		if (lc->verbose) fprintf(stdout, "Could not open file %s or %s\n", item_path, gz_path);
		e = GF_IO_ERR;
	} else {
		while ((read = fread(buffer, 1, 100, file))) gzwrite(gz, buffer, read);
		fclose(file);
		gzclose(gz);
		file = fopen(gz_path, "rb");
		fseek(file, 0, SEEK_END);
		*size = (u32)ftell(file);
		fclose(file);
		if (*size == 0) {
			if (lc->verbose) fprintf(stdout, "File %s has a size of 0\n", gz_path);
			e = GF_IO_ERR;
		}
	}
	return e;
}

GF_Err get_gz_loadtime(GF_LoadCompare *lc, char *item_name, char *item_path, u32 *loadtime, Bool useLibXML)
{
	char gz_path[256];
	GF_Err e = GF_OK;
	*loadtime = 0;

	strcpy(gz_path, item_name);
	strcat(gz_path, "z");

	if (useLibXML) {
		e = libxml_load_svg(lc, gz_path, loadtime);
	} else {
		e = gpacctx_load_file(lc, gz_path, loadtime);
	}
	return e;
}

void print_load_data(GF_LoadCompare *lc, LoadData *ld)
{
	if (lc->verbose) fprintf(stdout, "Processing %s\n", ld->filename);
	fprintf(lc->out, "%s\t", ld->filename);

	if (lc->verbose) fprintf(stdout, "File Size %d\n", ld->size);
	fprintf(lc->out, "%d\t", ld->size);

	if (lc->verbose) fprintf(stdout, "GPAC XML Load Time %d\n", ld->gpacxml_loadtime);
	fprintf(lc->out, "%d\t", ld->gpacxml_loadtime);

	if (lc->verbose) fprintf(stdout, "LibXML Load Time %d \n", ld->libxml_loadtime);
	fprintf(lc->out, "%d\t", ld->libxml_loadtime);

	if (lc->verbose) fprintf(stdout, "GZ Size %d\n", ld->gz_size);
	fprintf(lc->out, "%d\t", ld->gz_size);

	if (lc->verbose) fprintf(stdout, "GZ Load Time %d\n", ld->gpacxml_gz_loadtime);
	fprintf(lc->out, "%d\t", ld->gpacxml_gz_loadtime);

	if (lc->verbose) fprintf(stdout, "LibXML GZ Load Time %d\n", ld->libxml_gz_loadtime);
	fprintf(lc->out, "%d\t", ld->libxml_gz_loadtime);

	if (lc->verbose) fprintf(stdout, "MP4 Track Size %d\n", ld->track_size);
	fprintf(lc->out, "%d\t", ld->track_size);

	if (lc->verbose) fprintf(stdout, "MP4 Track Load Time %d\n", ld->track_loadtime);
	fprintf(lc->out, "%d\t", ld->track_loadtime);

	if (lc->verbose) fprintf(stdout, "Decoded Size %d\n", ld->decoded_size);
	fprintf(lc->out, "%d\t", ld->decoded_size);

	if (lc->verbose) fprintf(stdout, "Decoded Load Time %d \n", ld->decoded_loadtime);
	fprintf(lc->out, "%d\t", ld->decoded_loadtime);

	if (lc->verbose) fprintf(stdout, "Done %s\n", ld->filename);
	fprintf(lc->out, "\n");
	fflush(lc->out);
}

Bool loadcompare_one(void *cbck, char *item_name, char *item_path)
{
	GF_Err e;
	GF_LoadCompare *lc = cbck;
	u32 loadtime;
	LoadData *ld;

	if (lc->repeat_index == 0) {
		GF_SAFEALLOC(ld, sizeof(LoadData));
		gf_list_add(lc->data, ld);
		strcpy(ld->filename, item_name);

		e = get_size(lc, item_name, item_path, &ld->size);
		if (e) return 1;

		e = create_gz_file(lc, item_name, item_path, &ld->gz_size);
		if (e) return 1;

		e = create_laser_mp4(lc, item_name, item_path, &ld->track_size);
		if (e) return 1;

	} else {
		LoadData *tmp;
		u32 pos = 0;
		ld = NULL;
		while (tmp = gf_list_enum(lc->data, &pos)) {
			if (!strcmp(tmp->filename, item_name)) {
				ld = tmp;
				break;
			}
		}
		if (ld == NULL) return 1;
	}


	if (lc->type == SVG) {
		/* GPAC XML loader */
		e = gpacctx_load_file(lc, item_path, &loadtime);
		if (e) return 1;
		ld->gpacxml_loadtime += loadtime;

		e = get_gz_loadtime(lc, item_name, item_path, &loadtime, 0);
		if (e) return 1;
		ld->gpacxml_gz_loadtime += loadtime;

		/* LibXML and LibXML GZ loadings */
		e = libxml_load_svg(lc, item_path, &loadtime);
		if (e) return 1;
		ld->libxml_loadtime += loadtime;

		e = get_gz_loadtime(lc, item_name, item_path, &loadtime, 1);
		if (e) return 1;
		ld->libxml_gz_loadtime += loadtime;

		/* MP4 Loading */
		e = get_mp4_loadtime(lc, item_name, item_path, &loadtime);
		if (e) return 1;
		ld->track_loadtime += loadtime;

		/*		e = get_decoded_svg_loadtime_and_size(lc, item_name, item_path, &loadtime, &ld->decoded_size);
				if (e) return 1;
				ld->decoded_loadtime += loadtime;*/

	} else if (lc->type == XMT) {
		e = gpacctx_load_file(lc, item_path, &loadtime);
		if (e) return 1;
		ld->gpacxml_loadtime += loadtime;
	}

	if (!lc->spread_repeat) {
		print_load_data(lc, ld);
		gf_free(ld);
	}
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
		} else if (!stricmp(arg, "-regenerate")) {
			lc.regenerate = 1;
		} else if (!stricmp(arg, "-xmt")) {
			lc.type = XMT;
		} else if (!stricmp(arg, "-svg")) {
			lc.type = SVG;
		} else if (!stricmp(arg, "-spread_repeat")) {
			lc.spread_repeat = 1;
		} else if (!stricmp(arg, "-verbose")) {
			lc.verbose = (u32)atoi(argv[i+1]);
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

	if (lc.type == SVG) {
		fprintf(lc.out,"File Name\tSVG Size\tSVG Load Time\tLibXML Load Time\tSVGZ Size\tSVGZ Load Time\tLibXML GZ Load Time\tMP4 Size\tMP4 Load Time\tDecoded SVG Size\tDecoded SVG Load Time\n");
	} else if (lc.type == XMT) {
		fprintf(lc.out,"File Name\tXMT Size\tXMT Load Time\tBT Size\tBT Load Time\n");
	}

	lc.data = gf_list_new();

	if (single) {
		LoadData *ld;
		char *tmp = strrchr(in, GF_PATH_SEPARATOR);
		loadcompare_one(&lc, tmp+1, in);
		ld = gf_list_get(lc.data, 0);
		print_load_data(&lc, ld);
		gf_free(ld);
	} else {
		if (lc.spread_repeat) {
			for (lc.repeat_index = 0; lc.repeat_index < lc.nbloads; lc.repeat_index ++) {
				if (lc.verbose) fprintf(stdout, "Loop %d\n", lc.repeat_index);
				if (lc.type == SVG) {
					gf_enum_directory(in, 0, loadcompare_one, &lc, "svg");
				} else if (lc.type == XMT) {
					gf_enum_directory(in, 0, loadcompare_one, &lc, "xmt");
				}
			}
			for (i=0; i<gf_list_count(lc.data); i++) {
				LoadData *ld = gf_list_get(lc.data, i);
				print_load_data(&lc, ld);
				gf_free(ld);
			}
		} else {
			if (lc.type == SVG) {
				gf_enum_directory(in, 0, loadcompare_one, &lc, "svg");
			} else if (lc.type == XMT) {
				gf_enum_directory(in, 0, loadcompare_one, &lc, "xmt");
			}
		}
	}
	gf_list_del(lc.data);

	if (lc.out) fclose(lc.out);
	gf_sys_close();
	return 0;
}

