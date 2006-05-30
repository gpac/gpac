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

typedef struct {
	FILE *out;
} GF_LoadCompare;


u32 loadonefile(char *item_name, Bool is_mp4) 
{
	GF_Err e;
	u32 starttime,endtime, totaltime, i; 
	GF_SceneLoader load;

	totaltime = 0;
	
	for (i =0; i<5; i++) {
		memset(&load, 0, sizeof(GF_SceneLoader));
		load.ctx = gf_sm_new(gf_sg_new());
		load.fileName = item_name;
		if (is_mp4) load.isom = gf_isom_open(item_name, GF_ISOM_OPEN_READ, NULL);
		starttime = gf_sys_clock();

		e = gf_sm_load_init(&load);
		if (e) return 0;

		e = gf_sm_load_run(&load);
		if (!e) gf_sm_load_done(&load);
		endtime = gf_sys_clock();
		totaltime += endtime-starttime;

		gf_sm_del(load.ctx);
		if (is_mp4) gf_isom_close(load.isom);
	}

	return totaltime;
}

Bool loadcompare_one(void *cbck, char *item_name, char *item_path)
{
	char name[100];
	char *tmp;
	GF_LoadCompare *lc = cbck;

	fprintf(stdout,"%s\n", item_name);
	fprintf(lc->out,"%s", item_name);

	/* SVG */
	fprintf(lc->out,"\t%d", loadonefile(item_name, 0));
	/* SVGZ */
    if (0) {
		strcpy(name, (const char*)item_name);
		fprintf(lc->out,"\t%d", loadonefile(strcat(name, "z"), 0));
	}
	/* MP4 */
	strcpy(name, (const char*)item_name);
	tmp = strrchr(name, '.');
	strcpy(tmp, "_0.mp4");
	fprintf(lc->out,"\t%d", loadonefile(name, 1));

	fprintf(lc->out,"\n");
	return 0;
}

int main(int argc, char **argv)
{
	GF_LoadCompare lc;
	
	gf_sys_init();
	memset(&lc, 0, sizeof(GF_LoadCompare));

	lc.out = fopen(argv[1], "wt");
	if (!lc.out) return -1;

	fprintf(lc.out,"File Name\tSVG Load Time\tGZIP Load Time\tMP4 Load Time\n");

//	loadcompare_one(&lc, "batik3D.svg", NULL);
	gf_enum_directory(argv[2], 0, loadcompare_one, &lc, "svg");
		
	if (lc.out) fclose(lc.out);
	gf_sys_close();
	return 0;
}

