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

#include <gpac/base_coding.h>

//#define ADOBE_INLINED_BOOTSTRAP

struct __tag_adobe_stream
{
	FILE *f;
	const char *id;
	const char *base_url;
	u32 bitrate;
};

struct __tag_adobe_multirate
{
	FILE *f;
	const char *id;
	const char *base_url;
	GF_List *streams;
};

static GF_Err adobe_gen_stream_manifest(AdobeStream *as) 
{ 
	fprintf(as->f, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n"); 
	fprintf(as->f, "<manifest xmlns=\"http://ns.adobe.com/f4m/1.0\">\n"); 
	fprintf(as->f, "<id>%s</id>\n", as->id); 
	if (as->base_url) 
		fprintf(as->f, "<baseURL>%s</baseURL>\n", as->base_url); 
	fprintf(as->f, "<bootstrapInfo profile=\"named\" id=\"boot_%s_%d\" url=\"%s_%d.bootstrap\"/>\n", as->id, as->bitrate, as->id, as->bitrate); 
	fprintf(as->f, "<media url=\"%s_%u_\" bitrate=\"%u\" bootstrapInfoId=\"boot_%s_%d\"/>\n", as->id, as->bitrate, as->bitrate, as->id, as->bitrate); 
	fprintf(as->f, "</manifest>\n"); 

	return GF_OK; 
}

AdobeMultirate *adobe_alloc_multirate_manifest(char *id)
{
	AdobeMultirate *am = gf_calloc(1, sizeof(AdobeMultirate));
	char filename[GF_MAX_PATH];

	//default init
	am->base_url = "http://localhost/hds/tmp";
	am->id = id;
	sprintf(filename, "%s.f4m", am->id);
	am->f = fopen(filename, "wt");
	if (!am->f) {
		fprintf(stderr, "Couldn't create Adobe multirate manifest file: %s\n", filename);
		assert(0);
		gf_free(am);
		return NULL;
	}
	am->streams = gf_list_new();

	//create a fake stream
	{
		AdobeStream *as = gf_calloc(1, sizeof(AdobeStream));
		as->id = "HD";
		as->bitrate = 100;
		sprintf(filename, "%s_%s_%d.f4m", am->id, as->id, as->bitrate); 
		as->f = fopen(filename, "wt"); 
		if (!as->f) { 
			fprintf(stderr, "Couldn't create Adobe stream manifest file: %s\n", filename); 
			assert(0); 
			gf_list_del(am->streams); 
			gf_free(as); 
			gf_free(am); 
			return NULL; 
		} 
		gf_list_add(am->streams, as);
	}

	return am;
}

void adobe_free_multirate_manifest(AdobeMultirate *am)
{
	u32 i;

	if (am->f)
		fclose(am->f);

	for (i=0; i<gf_list_count(am->streams); i++) {
		AdobeStream *as = gf_list_get(am->streams, i);
		assert(as);
		if (as->f) 
			fclose(as->f); 
		//TODO: base_url and id may be stored as gf_strdup in the future
		gf_list_rem(am->streams, i);
		gf_free(as);
	}
	gf_list_del(am->streams);

	gf_free(am);
}

GF_Err adobe_gen_multirate_manifest(AdobeMultirate* am, char *bootstrap, size_t bootstrap_size)
{
	GF_Err e;
	u32 i;
#ifdef ADOBE_INLINED_BOOTSTRAP
	char bootstrap64[GF_MAX_PATH];
	u32 bootstrap64_len;
#endif

	fprintf(am->f, "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n");
	fprintf(am->f, "<manifest xmlns=\"http://ns.adobe.com/f4m/2.0\">\n");
	fprintf(am->f, "<id>%s</id>\n", am->id);
	fprintf(am->f, "<baseURL>%s</baseURL>\n", am->base_url);
    fprintf(am->f, "<streamType>live</streamType>\n");

	assert(am->streams);
	for (i=0; i<gf_list_count(am->streams); i++) {
		AdobeStream *as = gf_list_get(am->streams, i);
		assert(as);
#ifdef ADOBE_INLINED_BOOTSTRAP
		fprintf(am->f, "<bootstrapInfo profile=\"named\" id=\"boot_%s_%d\">\n", as->id, as->bitrate);
		bootstrap64_len = gf_base64_encode(bootstrap, bootstrap_size, bootstrap64, GF_MAX_PATH);
		fwrite(bootstrap64, bootstrap64_len, 1, am->f);
		if (bootstrap64_len >= GF_MAX_PATH) {
			fprintf(stderr, "Bootstrap may have been truncated for stream %s_%d.\n", as->id, as->bitrate);
			assert(0);
		}
		fprintf(am->f, "\n</bootstrapInfo>\n");
#else
		{
			char filename[GF_MAX_PATH];
			FILE *bstfile;
			sprintf(filename, "%s_%d.bootstrap", as->id, as->bitrate);
			bstfile = fopen(filename, "wb");
			gf_fwrite(bootstrap, bootstrap_size, 1, bstfile);
			fclose(bstfile);
		}
#endif
		e = adobe_gen_stream_manifest(as); 
		if (!e) { 
			if (!am->base_url && !as->base_url) 
				fprintf(stderr, "Warning: no base_url specified\n"); 

			fprintf(am->f, "<media href=\"%s_%s_%d.f4m\" bitrate=\"%d\"/>\n", am->id, as->id, as->bitrate, as->bitrate); 
		}
	}
	fprintf(am->f, "</manifest>\n");

	return GF_OK;
}
