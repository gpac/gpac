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

//we need to write Adobe custom boxes
#include <gpac/internal/isomedia_dev.h>

GF_Err adobize_segment(GF_ISOFile *isom_file, AdobeHDSCtx *ctx)
{
	GF_Err e;
	GF_BitStream *bs;

	GF_AdobeFragRandomAccessBox *afra = (GF_AdobeFragRandomAccessBox*)	afra_New();
	GF_AfraEntry *ae                  = (GF_AfraEntry*)					gf_calloc(1, sizeof(GF_AfraEntry));
	GF_AdobeBootstrapInfoBox *abst    = (GF_AdobeBootstrapInfoBox*)		abst_New();
	GF_AdobeSegmentRunTableBox *asrt  = (GF_AdobeSegmentRunTableBox*)	asrt_New();
	GF_AdobeSegmentRunEntry *asre     = (GF_AdobeSegmentRunEntry*)		gf_calloc(1, sizeof(GF_AdobeSegmentRunEntry));
	GF_AdobeFragmentRunTableBox *afrt = (GF_AdobeFragmentRunTableBox*)	afrt_New();
	GF_AdobeFragmentRunEntry *afre    = (GF_AdobeFragmentRunEntry*)		gf_calloc(1, sizeof(GF_AdobeFragmentRunEntry));

	u64 init_seg_time = ctx->curr_time;
	u32 seg_duration = (u32)gf_isom_get_duration(isom_file);

	//update context
	ctx->curr_time += seg_duration;

	//Adobe specific boxes
	//Random Access
	afra->type = GF_4CC('a', 'f', 'r', 'a');
	afra->version = 0;
	afra->flags = 0;
	afra->long_ids = 1;
	afra->long_offsets = 1;
	afra->global_entries = 0;
	afra->time_scale = gf_isom_get_timescale(isom_file);

	afra->entry_count = 1;
	ae->time = init_seg_time;
	ae->offset = 3999;
	gf_list_add(afra->local_access_entries, ae);

	afra->global_entries = 0;
	afra->global_entry_count = 0;

	e = gf_list_add(isom_file->TopBoxes, afra);
	if (e) {
		fprintf(stderr, "Impossible to write AFRA box: %s\n", gf_error_to_string(e));
		assert(0);
		return e;
	}

	//Bootstrap Info
	abst->type = GF_4CC('a', 'b', 's', 't');
	abst->version = 0;
	abst->flags = 0;
	abst->bootstrapinfo_version = 1;
	abst->profile = 0;
	abst->live = 1;
	abst->update = 0;
	abst->time_scale = gf_isom_get_timescale(isom_file);
	abst->current_media_time = init_seg_time+seg_duration;
	abst->smpte_time_code_offset = 0;

	abst->movie_identifier = NULL;
	abst->drm_data = NULL;
	abst->meta_data = NULL;

	abst->server_entry_count = 0;
	abst->quality_entry_count = 0;

	abst->segment_run_table_count = 1;
	{
		//Segment Run
		asrt->type = GF_4CC('a', 's', 'r', 't');
		asrt->version = 0;
		asrt->flags = 0;
		asrt->segment_run_entry_count = 1;
		{
			asre->first_segment = ctx->segnum;
			asre->fragment_per_segment = 1;
		}
		e = gf_list_add(asrt->segment_run_entry_table, asre);
		if (e) {
			fprintf(stderr, "Impossible to write ASR Entry: %s\n", gf_error_to_string(e));
			assert(0);
			return e;
		}
	}
	e = gf_list_add(abst->segment_run_table_entries, asrt);
	if (e) {
		fprintf(stderr, "Impossible to write ASRT box: %s\n", gf_error_to_string(e));
		assert(0);
		return e;
	}

	abst->fragment_run_table_count = 1;
	{
		//Fragment Run
		afrt->type = GF_4CC('a', 'f', 'r', 't');
		afrt->version = 0;
		afrt->flags = 0;
		afrt->timescale = gf_isom_get_timescale(isom_file);
		afrt->fragment_run_entry_count = 1;
		{
			afre->first_fragment = 1;
			afre->first_fragment_timestamp = 0;
			afre->fragment_duration = seg_duration;
		}
		e = gf_list_add(afrt->fragment_run_entry_table, afre);
		if (e) {
			fprintf(stderr, "Impossible to write AFR Entry: %s\n", gf_error_to_string(e));
			assert(0);
			return e;
		}
	}
	e = gf_list_add(abst->fragment_run_table_entries, afrt);
	if (e) {
		fprintf(stderr, "Impossible to write AFRT box: %s\n", gf_error_to_string(e));
		assert(0);
		return e;
	}

	e = gf_list_add(isom_file->TopBoxes, abst);
	if (e) {
		fprintf(stderr, "Impossible to write ABST box: %s\n", gf_error_to_string(e));
		assert(0);
		return e;
	}

	e = abst_Size((GF_Box*)abst);
	if (e) {
		fprintf(stderr, "Impossible to compute ABST box size: %s\n", gf_error_to_string(e));
		assert(0);
		return e;
	}
	ctx->bootstrap_size = (size_t)abst->size;
	ctx->bootstrap = gf_malloc(ctx->bootstrap_size);
	bs = gf_bs_new(ctx->bootstrap, ctx->bootstrap_size, GF_BITSTREAM_WRITE);
	e = abst_Write((GF_Box*)abst, bs);
	if (e) {
		fprintf(stderr, "Impossible to code the ABST box: %s\n", gf_error_to_string(e));
		assert(0);
		gf_bs_del(bs);
		return e;
	}
	gf_bs_del(bs);

	//set brands as reversed engineered from f4v files
	/*e = gf_isom_reset_alt_brands(isom_file);
	if (e) {
		fprintf(stderr, "Warning: couldn't reset ISOM brands: %s\n", gf_error_to_string(e));
		assert(0);
	}*/
	gf_isom_set_brand_info(isom_file, GF_4CC('f','4','v',' '), 1);
	gf_isom_modify_alternate_brand(isom_file, GF_4CC('i','s','o','m'), 1);
	gf_isom_modify_alternate_brand(isom_file, GF_4CC('m','p','4','2'), 1);
	gf_isom_modify_alternate_brand(isom_file, GF_4CC('m','4','v',' '), 1);

	return GF_OK;
}