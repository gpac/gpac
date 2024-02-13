/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom Paris 2024
 *					All rights reserved
 *
 *  This file is part of GPAC - test MP4 muxer
 *
 */

#include <gpac/tools.h>
#include <gpac/isomedia.h>


static GF_Err _on_data(void *cbk, u8 *data, u32 block_size, void *cbk_data, u32 cbk_magic)
{
	FILE *f = cbk;
	gf_fwrite(data, block_size, f);
	return GF_OK;
}
static GF_Err _on_data_patch(void *cbk, u8 *block, u32 block_size, u64 block_offset, Bool is_insert)
{
	FILE *f = cbk;
	u64 pos = gf_ftell(f);
	gf_fseek(f, block_offset, SEEK_SET);
	gf_fwrite(block, block_size, f);
	gf_fseek(f, pos, SEEK_SET);
	return GF_OK;
}

void mp4mux_api_test(u32 frag_type, Bool moov_first, const char *fileName)
{
	GF_Err e;
	//open target file
	FILE *f = gf_fopen(fileName, "w");
	//open movie
	GF_ISOFile *nf = gf_isom_open("_gpac_isobmff_redirect", (!frag_type && moov_first) ? GF_ISOM_WRITE_EDIT : GF_ISOM_OPEN_WRITE, NULL);

	//set storage mode to interlleaved
	if (moov_first && !frag_type) {
		e = gf_isom_set_storage_mode(nf, GF_ISOM_STORE_INTERLEAVED); //or GF_ISOM_STORE_STREAMABLE since we only have one track
		assert(!e);
	}
	//setup our callbacks, we don't use the last_block callback
	e =	gf_isom_set_write_callback(nf, _on_data, _on_data_patch, NULL, f, 1000);
	assert(!e);

	//movie setup
	e = gf_isom_set_brand_info(nf, GF_ISOM_BRAND_MP42, 0);
	assert(!e);
	e = gf_isom_set_timescale(nf, 1000);
	assert(!e);
	u32 track_id = gf_isom_new_track(nf, 0, GF_ISOM_MEDIA_SUBT, 1000);
	assert(track_id);
	e = gf_isom_set_track_enabled(nf, track_id, GF_TRUE);
	assert(!e);
	u32 i, di;
	e = gf_isom_new_stxt_description(nf, 1, GF_ISOM_SUBTYPE_STXT, "text/text", NULL, NULL, &di);
	assert(!e);

	//setup fragmentation
	if (frag_type) {
		e = gf_isom_setup_track_fragment(nf, track_id, di, 2000, 0, 1, 0, 0, 0);
		assert(!e);
		//will write init segment
		e = gf_isom_finalize_for_fragment(nf, (frag_type==2) ? 1 : 0, GF_TRUE);
		assert(!e);
	}
	//add samples
	u64 first_dts=0;
	u64 dts=0;
	u32 duration=2000;
	for (i=0; i<10000; i++) {
		char szTxt[100];
		GF_ISOSample s;

		if (frag_type && ((i%100)==0)) {
			if (frag_type==2) {
				if (i) {
					e = gf_isom_close_segment(nf, 0, track_id, first_dts, 0, dts+duration, GF_FALSE, GF_FALSE, GF_FALSE, GF_FALSE, 0, NULL, NULL, NULL);
					assert(!e);
				}
				//use either NULL if you store in same file or "_gpac_isobmff_redirect" for multiple files (to setup styp and indexing)
				e = gf_isom_start_segment(nf, NULL, GF_FALSE);
				assert(!e);
				first_dts = dts;
				//in this demo, one fragment per segment
			}

			//starting a fragment will flush all pending data
			e = gf_isom_start_fragment(nf, GF_ISOM_FRAG_MOOF_FIRST);
			assert(!e);
		}

		sprintf(szTxt, "Text sample #%d", i+1);
		memset(&s, 0, sizeof(GF_ISOSample));
		s.dataLength = (u32) strlen(szTxt);
		s.data = (u8 *)szTxt;
		s.DTS = dts;
		s.IsRAP = 1;
		if (frag_type) {
			e = gf_isom_fragment_add_sample(nf, track_id, &s, di, 2000, 0, 0, GF_FALSE);
		} else {
			e = gf_isom_add_sample(nf, 1, di, &s);
		}
		assert(!e);
		dts+=duration;
	}
	//close segment
	if (frag_type==2) {
		e = gf_isom_close_segment(nf, 0, track_id, first_dts, 0, dts+duration, GF_FALSE, GF_FALSE, GF_FALSE, GF_FALSE, 0, NULL, NULL, NULL);
		assert(!e);
	}

	//flush all pending data and delete
	e = gf_isom_close(nf);
	assert(!e);
	gf_fclose(f);
}

int main(int argc, char **argv)
{

	gf_sys_init(0, "0");

	//mux using callbacks, no fragments and mdat first
	mp4mux_api_test(0, GF_FALSE, "mux_flat.mp4");
	//mux using callbacks, no fragments and moov first
	mp4mux_api_test(0, GF_TRUE, "mux_inter.mp4");
	//mux using callbacks and fragments
	mp4mux_api_test(1, GF_FALSE, "mux_frag.mp4");
	//mux using callbacks and segments
	mp4mux_api_test(2, GF_FALSE, "mux_seg.mp4");

	gf_sys_close();
}

