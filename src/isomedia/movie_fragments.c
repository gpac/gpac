/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
 *					All rights reserved
 *
 *  This file is part of GPAC / ISO Media File Format sub-project
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

#include <gpac/internal/isomedia_dev.h>

#ifndef GPAC_DISABLE_ISOM

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS

GF_TrackExtendsBox *GetTrex(GF_MovieBox *moov, GF_ISOTrackID TrackID)
{
	u32 i;
	GF_TrackExtendsBox *trex;
	i=0;
	while ((trex = (GF_TrackExtendsBox *)gf_list_enum(moov->mvex->TrackExList, &i))) {
		if (trex->trackID == TrackID) return trex;
	}
	return NULL;
}


GF_TrackFragmentBox *gf_isom_get_traf(GF_ISOFile *mov, GF_ISOTrackID TrackID)
{
	u32 i;
	if (!mov->moof) return NULL;

	//reverse browse the TRAFs, as there may be more than one per track ...
	for (i=gf_list_count(mov->moof->TrackList); i>0; i--) {
		GF_TrackFragmentBox *traf = (GF_TrackFragmentBox *)gf_list_get(mov->moof->TrackList, i-1);
		if (traf->tfhd->trackID == TrackID) return traf;
	}
	return NULL;
}


#ifndef GPAC_DISABLE_ISOM_WRITE
GF_Err gf_isom_set_movie_duration(GF_ISOFile *movie, u64 duration, Bool remove_mehd)
{
	if (!movie || !movie->moov || !movie->moov->mvex) return GF_BAD_PARAM;

	if (remove_mehd) {
		if (!movie->moov->mvex->mehd) {
			gf_isom_box_del_parent(&movie->moov->mvex->child_boxes, (GF_Box*)movie->moov->mvex->mehd);
			movie->moov->mvex->mehd = NULL;
		}
	} else {
		if (!movie->moov->mvex->mehd) {
			movie->moov->mvex->mehd = (GF_MovieExtendsHeaderBox *) gf_isom_box_new_parent(&movie->moov->mvex->child_boxes, GF_ISOM_BOX_TYPE_MEHD);
			if (!movie->moov->mvex->mehd) return GF_OUT_OF_MEM;
		}
		movie->moov->mvex->mehd->fragment_duration = duration;
	}
	movie->moov->mvhd->duration = 0;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_finalize_for_fragment(GF_ISOFile *movie, u32 media_segment_type, Bool mvex_after_tracks)
{
	GF_Err e;
	u32 i;
	Bool store_file = GF_TRUE;
	GF_TrackExtendsBox *trex;
	if (!movie || !movie->moov) return GF_BAD_PARAM;

#if 0
	if (movie->openMode==GF_ISOM_OPEN_CAT_FRAGMENTS) {
		/*from now on we are in write mode*/
		movie->openMode = GF_ISOM_OPEN_WRITE;
		store_file = GF_FALSE;
		movie->append_segment = GF_TRUE;
	} else
#endif
	{
		movie->NextMoofNumber = 1;
	}
	movie->moov->mvex_after_traks = mvex_after_tracks;
	
	//this is only allowed in write mode
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	if (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) return GF_OK;
	movie->FragmentsFlags = 0;

	if (store_file) {
		/* add DASH brand if requested*/
		if (media_segment_type)
			gf_isom_modify_alternate_brand(movie, GF_ISOM_BRAND_DASH, GF_TRUE);

		if (!movie->moov->mvex->mehd || !movie->moov->mvex->mehd->fragment_duration) {
			//update durations
			gf_isom_get_duration(movie);
		}

		i=0;
		while ((trex = (GF_TrackExtendsBox *)gf_list_enum(movie->moov->mvex->TrackExList, &i))) {
			if (trex->type != GF_ISOM_BOX_TYPE_TREX) continue;
			if (trex->track->Media->information->sampleTable->CompositionToDecode) {
				u32 k=0;
				GF_TrackExtensionPropertiesBox *trep;
				while ((trep = (GF_TrackExtensionPropertiesBox*) gf_list_enum(movie->moov->mvex->TrackExPropList, &k))) {
					if (trep->trackID == trex->trackID) break;
				}

				if (!trep) {
					trep = (GF_TrackExtensionPropertiesBox*) gf_isom_box_new_parent(&movie->moov->mvex->child_boxes, GF_ISOM_BOX_TYPE_TREP);
					if (!trep) return GF_OUT_OF_MEM;
					trep->trackID = trex->trackID;
					gf_list_add(movie->moov->mvex->TrackExPropList, trep);
				}

				if (!trex->track->Media->information->sampleTable->SampleSize || ! trex->track->Media->information->sampleTable->SampleSize->sampleCount) {
					gf_list_add(trep->child_boxes, trex->track->Media->information->sampleTable->CompositionToDecode);
					trex->track->Media->information->sampleTable->CompositionToDecode = NULL;
				} else {
					GF_CompositionToDecodeBox *cslg;

					//clone it!
					GF_SAFEALLOC(cslg, GF_CompositionToDecodeBox);
					if (!cslg) return GF_OUT_OF_MEM;
					memcpy(cslg, trex->track->Media->information->sampleTable->CompositionToDecode, sizeof(GF_CompositionToDecodeBox) );
					cslg->child_boxes = gf_list_new();
					gf_list_add(trep->child_boxes, trex->track->Media->information->sampleTable->CompositionToDecode);
				}
			}

			if (movie->moov->mvex->mehd && movie->moov->mvex->mehd->fragment_duration) {
				trex->track->Header->duration = 0;
				Media_SetDuration(trex->track);
				if (trex->track->editBox && trex->track->editBox->editList) {
					GF_EdtsEntry *edts = gf_list_last(trex->track->editBox->editList->entryList);
					edts->segmentDuration = 0;
				}
			}
		}

		//write movie
		e = WriteToFile(movie, GF_TRUE);
		if (e) return e;

		if (movie->on_block_out) {
			gf_bs_seek(movie->editFileMap->bs, 0);
			gf_bs_truncate(movie->editFileMap->bs);
		}
	}

	//make sure we do have all we need. If not this is not an error, just consider
	//the file closed
	if (!movie->moov->mvex || !gf_list_count(movie->moov->mvex->TrackExList)) return GF_OK;

	i=0;
	while ((trex = (GF_TrackExtendsBox *)gf_list_enum(movie->moov->mvex->TrackExList, &i))) {
		if (!trex->trackID || !gf_isom_get_track_from_id(movie->moov, trex->trackID)) return GF_IO_ERR;
		//we could also check all our data refs are local but we'll do that at run time
		//in order to allow a mix of both (remote refs in MOOV and local in MVEX)

		//one thing that MUST be done is OD cross-dependencies. The movie fragment spec
		//is broken here, since it cannot allow dynamic insertion of new ESD and their
		//dependancies
	}

	//ok we are fine - note the data map is created at the beginning
	if (i) movie->FragmentsFlags |= GF_ISOM_FRAG_WRITE_READY;

	if (media_segment_type) {
		movie->use_segments = GF_TRUE;
		movie->moof_list = gf_list_new();
	} else if (movie->on_block_out) {
		movie->moof_list = gf_list_new();
	}

	/*set brands for segment*/

	/*"msdh": it's a media segment */
	gf_isom_set_brand_info(movie, GF_ISOM_BRAND_MSDH, 0);
	/*remove all brands	*/
	gf_isom_reset_alt_brands(movie);
	/*
		msdh: it's a media segment
		sims: it's a media segment with an SSIX
		msix: it's a media segment with an index
		lmsg: it's the last media segment
	*/

	return GF_OK;
}

GF_Err gf_isom_change_track_fragment_defaults(GF_ISOFile *movie, GF_ISOTrackID TrackID,
        u32 DefaultSampleDescriptionIndex,
        u32 DefaultSampleDuration,
        u32 DefaultSampleSize,
        u8 DefaultSampleIsSync,
        u8 DefaultSamplePadding,
        u16 DefaultDegradationPriority,
        u8 force_traf_flags)
{
	GF_MovieExtendsBox *mvex;
	GF_TrackExtendsBox *trex;
	GF_TrackBox *trak;

	if (!movie || !movie->moov) return GF_BAD_PARAM;
	//this is only allowed in write mode
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	trak = gf_isom_get_track_from_id(movie->moov, TrackID);
	if (!trak) return GF_BAD_PARAM;

	mvex = movie->moov->mvex;
	if (!mvex) return GF_BAD_PARAM;

	trex = GetTrex(movie->moov, TrackID);
	if (!trex)  return GF_BAD_PARAM;

	trex->def_sample_desc_index = DefaultSampleDescriptionIndex;
	trex->def_sample_duration = DefaultSampleDuration;
	trex->def_sample_size = DefaultSampleSize;
	trex->def_sample_flags = GF_ISOM_FORMAT_FRAG_FLAGS(DefaultSamplePadding, DefaultSampleIsSync, DefaultDegradationPriority);
	//if sample is sync by default, set sample_depends_on flags to 2 (does not depend on other samples)
	if (DefaultSampleIsSync) {
		trex->def_sample_flags |= (2<<24);
	}
	trex->cannot_use_default = GF_FALSE;

	if (force_traf_flags) {
		trex->cannot_use_default = GF_TRUE;
	} else if (DefaultSampleDescriptionIndex == 0 && DefaultSampleDuration == 0 && DefaultSampleSize == 0
		&& DefaultSampleIsSync == 0 && DefaultSamplePadding == 0 && DefaultDegradationPriority == 0) {
		trex->cannot_use_default = GF_TRUE;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_setup_track_fragment(GF_ISOFile *movie, GF_ISOTrackID TrackID,
                                    u32 DefaultSampleDescriptionIndex,
                                    u32 DefaultSampleDuration,
                                    u32 DefaultSampleSize,
                                    u8 DefaultSampleSyncFlags,
                                    u8 DefaultSamplePadding,
                                    u16 DefaultDegradationPriority,
                                    Bool force_traf_flags)
{
	GF_MovieExtendsBox *mvex;
	GF_TrackExtendsBox *trex;
	GF_TrackBox *trak;

	if (!movie || !movie->moov) return GF_BAD_PARAM;
	//this is only allowed in write mode
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;
	//and only at setup
	if (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) return GF_BAD_PARAM;


	trak = gf_isom_get_track_from_id(movie->moov, TrackID);
	if (!trak) return GF_BAD_PARAM;

	if (DefaultSampleSyncFlags & GF_ISOM_FRAG_USE_SYNC_TABLE) {
		DefaultSampleSyncFlags &= ~GF_ISOM_FRAG_USE_SYNC_TABLE;
		if (!trak->Media->information->sampleTable->SyncSample) {
			trak->Media->information->sampleTable->SyncSample = (GF_SyncSampleBox *) gf_isom_box_new_parent(&trak->Media->information->sampleTable->child_boxes, GF_ISOM_BOX_TYPE_STSS);
		}
	}

	//create MVEX if needed
	if (!movie->moov->mvex) {
		mvex = (GF_MovieExtendsBox *) gf_isom_box_new_parent(&movie->moov->child_boxes, GF_ISOM_BOX_TYPE_MVEX);
		if (!mvex) return GF_OUT_OF_MEM;
		moov_on_child_box((GF_Box*)movie->moov, (GF_Box *) mvex, GF_FALSE);
	} else {
		mvex = movie->moov->mvex;
	}
	if (!mvex->mehd) {
		mvex->mehd = (GF_MovieExtendsHeaderBox *) gf_isom_box_new_parent(&mvex->child_boxes, GF_ISOM_BOX_TYPE_MEHD);
		if (!mvex->mehd) return GF_OUT_OF_MEM;
	}

	trex = GetTrex(movie->moov, TrackID);
	if (!trex) {
		trex = (GF_TrackExtendsBox *) gf_isom_box_new_parent(&mvex->child_boxes, GF_ISOM_BOX_TYPE_TREX);
		if (!trex) return GF_OUT_OF_MEM;
		trex->trackID = TrackID;
		mvex_on_child_box((GF_Box*)mvex, (GF_Box *) trex, GF_FALSE);
	}
	trex->track = trak;
	return gf_isom_change_track_fragment_defaults(movie, TrackID, DefaultSampleDescriptionIndex, DefaultSampleDuration, DefaultSampleSize, DefaultSampleSyncFlags, DefaultSamplePadding, DefaultDegradationPriority, force_traf_flags);
}

#ifdef GF_ENABLE_CTRN
GF_EXPORT
GF_Err gf_isom_enable_traf_inherit(GF_ISOFile *movie, GF_ISOTrackID TrackID, GF_ISOTrackID BaseTrackID)
{
	GF_TrackBox *trak;
	GF_TrackExtendsBox *trex;
	GF_Err e=GF_OK;
	u32 track_num;
	if (!movie || !TrackID || !BaseTrackID)
		return GF_BAD_PARAM;
	trak = gf_isom_get_track_from_id(movie->moov, TrackID);
	if (!trak) return GF_BAD_PARAM;
	track_num = 1 + gf_list_find(movie->moov->trackList, trak);

	e = gf_isom_set_track_reference(movie, track_num, GF_ISOM_REF_TRIN, BaseTrackID);
	if (e) return e;

	trex = GetTrex(movie->moov, TrackID);
	if (!trex) return GF_BAD_PARAM;
	trex->inherit_from_traf_id = BaseTrackID;
	return GF_OK;
}
#endif

GF_EXPORT
GF_Err gf_isom_setup_track_fragment_template(GF_ISOFile *movie, GF_ISOTrackID TrackID, u8 *boxes, u32 boxes_size, u8 force_traf_flags)
{
	GF_MovieExtendsBox *mvex;
	GF_TrackBox *trak;
	GF_BitStream *bs;
	GF_Err e=GF_OK;
	trak = gf_isom_get_track_from_id(movie->moov, TrackID);
	if (!trak) return GF_BAD_PARAM;

	bs = gf_bs_new(boxes, boxes_size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		GF_Box *box=NULL;
		gf_isom_box_parse(&box, bs);
		if (!box) {
			e = GF_BAD_PARAM;
			break;
		}

		if (box->type==GF_ISOM_BOX_TYPE_TREX) {
			GF_TrackExtendsBox *trex_o=NULL;
			GF_TrackExtendsBox *trex = (GF_TrackExtendsBox *) box;

			//create MVEX if needed
			if (!movie->moov->mvex) {
				mvex = (GF_MovieExtendsBox *) gf_isom_box_new_parent(&movie->moov->child_boxes, GF_ISOM_BOX_TYPE_MVEX);
				moov_on_child_box((GF_Box*)movie->moov, (GF_Box *) mvex, GF_FALSE);
			} else {
				mvex = movie->moov->mvex;
			}
			if (!mvex->mehd) {
				mvex->mehd = (GF_MovieExtendsHeaderBox *) gf_isom_box_new_parent(&mvex->child_boxes, GF_ISOM_BOX_TYPE_MEHD);
			}

			trex_o = GetTrex(movie->moov, TrackID);
			if (trex_o) {
				gf_list_del_item(movie->moov->mvex->TrackExList, trex_o);
				gf_isom_box_del_parent(&movie->moov->mvex->child_boxes, (GF_Box *)trex_o);
			}
			trex->trackID = TrackID;
			trex->track = trak;
			if (force_traf_flags) trex->cannot_use_default = GF_TRUE;
			gf_list_add(mvex->child_boxes, trex);
			mvex_on_child_box((GF_Box*)mvex, (GF_Box *) trex, GF_FALSE);
		}
	}
	gf_bs_del(bs);
	return e;
}


u32 GetNumUsedValues(GF_TrackFragmentBox *traf, u32 value, u32 index)
{
	u32 i, j, NumValue = 0;
	GF_TrackFragmentRunBox *trun;

	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		for (j=0; j<trun->nb_samples; j++) {
			GF_TrunEntry *ent = &trun->samples[j];
			switch (index) {
			case 1:
				if (value == ent->Duration) NumValue ++;
				break;
			case 2:
				if (value == ent->size) NumValue ++;
				break;
			case 3:
				if (value == ent->flags) NumValue ++;
				break;
			}
		}
	}
	return NumValue;
}


void ComputeFragmentDefaults(GF_TrackFragmentBox *traf)
{
	u32 i, j;
	GF_TrackFragmentRunBox *trun;
	GF_TrunEntry *ent, *first_ent=NULL;

	//Duration default
	u32 def_dur=0;
	u32 def_size=0;
	u32 def_flags=0;
	u32 nb_samp=0;

	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		for (j=0; j<trun->nb_samples; j++) {
			ent = &trun->samples[j];
			nb_samp++;

			if (!first_ent) {
				first_ent = ent;
				def_dur = ent->Duration;
				def_size = ent->size;
				if (ent->nb_pack>1)
					def_size /= ent->nb_pack;
				continue;
			}
			//if more than 2 dur, we need the flag
			if (def_dur && (ent->Duration != def_dur)) def_dur=0;
			//if more than 2 size, we need the flag
			if (def_size) {
				u32 size = ent->size;
				if (ent->nb_pack>1)
					size /= ent->nb_pack;
				if (size != def_size) def_size=0;
			}
			//only check sample flags after first sample (first one uses first sample flags)
			if (nb_samp==2) def_flags = ent->flags;
			//if more than 2 sets of flags, we need one entry each
			else if (def_flags && (ent->flags != def_flags)) def_flags = 0;

			//no default possible
			if ((def_dur|def_size|def_flags) == 0)
				break;
		}
	}
	if (nb_samp==1) def_flags = first_ent->flags;

	if (def_dur && ((def_dur != traf->trex->def_sample_duration) || traf->trex->cannot_use_default ) ) {
		traf->tfhd->def_sample_duration = def_dur;
	}
	if (def_size && (def_size != traf->trex->def_sample_size)) {
		traf->tfhd->def_sample_size = def_size;
	}
	if (traf->trex->cannot_use_default || (def_flags && (def_flags != traf->trex->def_sample_flags))) {
		traf->tfhd->def_sample_flags = def_flags;
	}
}

GF_EXPORT
GF_Err gf_isom_set_fragment_option(GF_ISOFile *movie, GF_ISOTrackID TrackID, GF_ISOTrackFragmentOption Code, u32 Param)
{
	GF_TrackFragmentBox *traf;
	if (!movie || !movie->moov) return GF_BAD_PARAM;
	//this is only allowed in write mode
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	switch (Code) {
	case GF_ISOM_TRAF_EMPTY:
		traf = gf_isom_get_traf(movie, TrackID);
		if (!traf) return GF_BAD_PARAM;
		traf->tfhd->EmptyDuration = Param;
		break;
	case GF_ISOM_TRAF_RANDOM_ACCESS:
		traf = gf_isom_get_traf(movie, TrackID);
		if (!traf) return GF_BAD_PARAM;
		traf->IFrameSwitching = Param;
		break;
	case GF_ISOM_TRAF_DATA_CACHE:
		traf = gf_isom_get_traf(movie, TrackID);
		if (!traf) return GF_BAD_PARAM;
		//don't cache only one sample ...
		traf->DataCache = Param > 1 ? Param : 0;
		break;
	case GF_ISOM_TFHD_FORCE_MOOF_BASE_OFFSET:
		movie->force_moof_base_offset = Param;
		break;
	case GF_ISOM_TRAF_USE_SAMPLE_DEPS_BOX:
		traf = gf_isom_get_traf(movie, TrackID);
		if (!traf) return GF_BAD_PARAM;
		traf->use_sdtp = (u8) Param;
		break;
	case GF_ISOM_TRUN_FORCE:
		traf = gf_isom_get_traf(movie, TrackID);
		if (!traf) return GF_BAD_PARAM;
		traf->force_new_trun = 1;
		break;
	case GF_ISOM_TRUN_SET_INTERLEAVE_ID:
		traf = gf_isom_get_traf(movie, TrackID);
		if (!traf) return GF_BAD_PARAM;
		traf->DataCache = 1;
		traf->use_sample_interleave = 1;
		if (traf->interleave_id != Param) {
			traf->force_new_trun = 1;
			traf->interleave_id = Param;
		}
		break;
	case GF_ISOM_TRAF_TRUNS_FIRST:
		traf = gf_isom_get_traf(movie, TrackID);
		if (!traf) return GF_BAD_PARAM;
		traf->truns_first = Param;
		break;
	case GF_ISOM_TRAF_TRUN_V1:
		traf = gf_isom_get_traf(movie, TrackID);
		if (!traf) return GF_BAD_PARAM;
		traf->truns_v1 = Param;
		break;
	case GF_ISOM_TRAF_USE_LARGE_TFDT:
		traf = gf_isom_get_traf(movie, TrackID);
		if (!traf) return GF_BAD_PARAM;
		traf->large_tfdt = Param;
		movie->force_sidx_v1 = Param ? GF_TRUE : GF_FALSE;
		break;
	}
	return GF_OK;
}

//#define USE_BASE_DATA_OFFSET

void update_trun_offsets(GF_ISOFile *movie, s32 offset)
{
#ifndef USE_BASE_DATA_OFFSET
	u32 i, j;
	GF_TrackFragmentBox *traf;
	i=0;
	while ((traf = (GF_TrackFragmentBox*)gf_list_enum(movie->moof->TrackList, &i))) {
		GF_TrackFragmentRunBox *trun;
		/*remove base data*/
		traf->tfhd->base_data_offset = 0;
		j=0;
		while ((trun = (GF_TrackFragmentRunBox*)gf_list_enum(traf->TrackRuns, &j))) {
			if ((j==1) || traf->use_sample_interleave) {
				trun->data_offset += offset;
			} else {
				trun->data_offset = 0;
			}
		}
	}
#endif
}

static
u32 UpdateRuns(GF_ISOFile *movie, GF_TrackFragmentBox *traf)
{
	u32 sampleCount, i, j, RunSize, RunDur, RunFlags, NeedFlags, UseCTS;
	/* enum:
	   0 - use values per sample in the trun box
	   1 - use default values from track fragment header
	   2 - use default values from track extends header */
	u32 UseDefaultSize, UseDefaultDur, UseDefaultFlag;
	GF_TrackFragmentRunBox *trun;
	GF_TrunEntry *ent;

	sampleCount = 0;

#ifndef USE_BASE_DATA_OFFSET
	if (movie->use_segments) {
		traf->tfhd->flags = GF_ISOM_MOOF_BASE_OFFSET;
	} else
#endif
	{
		if (movie->force_moof_base_offset) {
			traf->tfhd->flags = GF_ISOM_MOOF_BASE_OFFSET;
		} else {
			traf->tfhd->flags = GF_ISOM_TRAF_BASE_OFFSET;
		}
	}

	//empty runs
	if (traf->tfhd->EmptyDuration) {
		while (gf_list_count(traf->TrackRuns)) {
			trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, 0);
			gf_list_rem(traf->TrackRuns, 0);
			gf_isom_box_del_parent(&traf->child_boxes, (GF_Box *)trun);
		}
		traf->tfhd->flags |= GF_ISOM_TRAF_DUR_EMPTY;
		if (traf->tfhd->EmptyDuration != traf->trex->def_sample_duration) {
			traf->tfhd->def_sample_duration = traf->tfhd->EmptyDuration;
			traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_DUR;
		}
		return 0;
	}


	UseDefaultSize = 0;
	UseDefaultDur = 0;
	UseDefaultFlag = 0;

	i=0;
	while ((trun = (GF_TrackFragmentRunBox *)gf_list_enum(traf->TrackRuns, &i))) {
		GF_TrunEntry *first_ent = NULL;
		RunSize = 0;
		RunDur = 0;
		RunFlags = 0;
		UseCTS = 0;
		NeedFlags = 0;

		//process all samples in run
		for (j=0; j<trun->nb_samples; j++) {
			ent = &trun->samples[j];
			if (!j) {
				first_ent = ent;
				RunSize = ent->size;
				if (ent->nb_pack) RunSize /= ent->nb_pack;
				RunDur = ent->Duration;
			}
			//we may have one entry only ...
			if (j || (trun->nb_samples==1)) {
				u32 ssize = ent->size;
				if (ent->nb_pack) ssize /= ent->nb_pack;

				//flags are only after first entry
				if (j==1 || (trun->nb_samples==1) ) RunFlags = ent->flags;

				if (ssize != RunSize) RunSize = 0;
				if (RunDur && (ent->Duration != RunDur))
					RunDur = 0;
				if (j && (RunFlags != ent->flags)) NeedFlags = 1;
			}
			if (ent->CTS_Offset) UseCTS = 1;
		}
		//empty list
		if (!first_ent) {
			i--;
			gf_list_rem(traf->TrackRuns, i);
			continue;
		}
		trun->flags = 0;

		//size checking
		//constant size, check if this is from current fragment default or global default
		if (RunSize && (traf->trex->def_sample_size == RunSize) && !traf->trex->cannot_use_default) {
			if (!UseDefaultSize) UseDefaultSize = 2;
			else if (UseDefaultSize==1) RunSize = 0;
		} else if (RunSize && (traf->tfhd->def_sample_size == RunSize)) {
			if (!UseDefaultSize) UseDefaultSize = 1;
			else if (UseDefaultSize==2) RunSize = 0;
		}
		//we could check for single entry runs and set the default size in the tfhd but
		//that's no bit saving...
		else {
			RunSize=0;
		}

		if (!RunSize) trun->flags |= GF_ISOM_TRUN_SIZE;

		//duration checking
		if (RunDur && (traf->trex->def_sample_duration == RunDur) && !traf->trex->cannot_use_default) {
			if (!UseDefaultDur) UseDefaultDur = 2;
			else if (UseDefaultDur==1) RunDur = 0;
		} else if (RunDur && (traf->tfhd->def_sample_duration == RunDur)) {
			if (!UseDefaultDur) UseDefaultDur = 1;
			else if (UseDefaultDur==2) RunDur = 0;
		}
		if (!RunDur) trun->flags |= GF_ISOM_TRUN_DURATION;

		//flag checking
		if (!NeedFlags) {
			// all samples flags are the same after the 2nd entry
			if (RunFlags == traf->trex->def_sample_flags && !traf->trex->cannot_use_default) {
				/* this run can use trex flags */
				if (!UseDefaultFlag) {
					/* if all previous runs used explicit flags per sample, we can still use trex flags for this run */
					UseDefaultFlag = 2;
				} else if (UseDefaultFlag==1) {
					/* otherwise if one of the previous runs did use tfhd flags,
					we have no choice but to explicitly use flags per sample for this run */
					NeedFlags = GF_TRUE;
				}
			} else if (RunFlags == traf->tfhd->def_sample_flags) {
				/* this run can use tfhd flags */
				if (!UseDefaultFlag) {
					/* if all previous runs used explicit flags per sample, we can still use tfhd flags for this run */
					UseDefaultFlag = 1;
				} else if(UseDefaultFlag==2) {
					/* otherwise if one of the previous runs did use trex flags,
					we have no choice but to explicitly use flags per sample for this run */
					NeedFlags = GF_TRUE;
				}
			} else {
				/* the flags for the 2nd and following entries are different from trex and tfhd default values
				   (possible case: 2 samples in trun, and first sample was used to set default flags) */
				NeedFlags = GF_TRUE;
			}
		}
		if (NeedFlags) {
			//one flags entry per sample only
			trun->flags |= GF_ISOM_TRUN_FLAGS;
		} else {
			/* this run can use default flags for the 2nd and following entries,
			   we just need to check if the first entry flags need to be singled out*/
			if (first_ent->flags != RunFlags) {
				trun->flags |= GF_ISOM_TRUN_FIRST_FLAG;
				//if not old arch write the flags
				//in old arch we write 0, which means all deps unknown and sync sample set
				if (!traf->no_sdtp_first_flags)
					trun->first_sample_flags = first_ent->flags;
			}
		}

		//CTS flag
		if (UseCTS) trun->flags |= GF_ISOM_TRUN_CTS_OFFSET;

		//run data offset if the offset indicated is 0 (first sample in this MDAT) don't
		//indicate it
		if (trun->data_offset)
			trun->flags |= GF_ISOM_TRUN_DATA_OFFSET;

		sampleCount += trun->sample_count;
	}

	//after all runs in the traf are processed, update TRAF flags
	if (UseDefaultSize==1)
		traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_SIZE;
	if (UseDefaultDur==1)
		traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_DUR;
	if (UseDefaultFlag==1)
		traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_FLAGS;
	if (traf->trex->cannot_use_default || (traf->tfhd->sample_desc_index != traf->trex->def_sample_desc_index))
		traf->tfhd->flags |= GF_ISOM_TRAF_SAMPLE_DESC;


	return sampleCount;
}

static s32 get_earliest_cts_following(GF_TrackFragmentBox *traf, u32 trun_idx, u32 samp_idx)
{
	GF_TrackFragmentRunBox *trun;
	GF_TrunEntry *ent;
	s32 earliest_cts = 0;
	s32 delta = 0;
	u32 j;
	u32 i=trun_idx;
	while ((trun = (GF_TrackFragmentRunBox*)gf_list_enum(traf->TrackRuns, &i))) {
		j=samp_idx;
		samp_idx=0;
		for (; j<trun->nb_samples; j++) {
			ent = &trun->samples[j];
			if (!j && (i==1)) {
				earliest_cts = ent->CTS_Offset;
			} else {
				s32 cts = ent->CTS_Offset+delta;
				if (earliest_cts > cts)
					earliest_cts = cts;
			}
			delta += ent->Duration;
		}
	}
	return earliest_cts;
}

static u32 moof_get_sap_info(GF_MovieFragmentBox *moof, GF_ISOTrackID refTrackID, u32 *sap_delta, Bool *starts_with_sap)
{
	u32 i, j, count, sap_type, sap_sample_num, cur_sample;
	s32 delta, earliest_cts;
	Bool first = GF_TRUE;
	GF_TrunEntry *ent;
	GF_TrackFragmentBox *traf=NULL;
	GF_TrackFragmentRunBox *trun;
	sap_type = 0;
	*sap_delta = 0;
	*starts_with_sap = GF_FALSE;
	for (i=0; i<gf_list_count(moof->TrackList); i++) {
		traf = (GF_TrackFragmentBox*)gf_list_get(moof->TrackList, i);
		if (traf->tfhd->trackID==refTrackID) break;
		traf=NULL;
	}
	if (!traf) return sap_type;
	earliest_cts = 0;

	/*first check if we have a roll/rap sample in this traf, and mark its sample count*/
	sap_type = 0;
	sap_sample_num = 0;
	/*check RAP and ROLL*/
	count = traf->sampleGroups ? gf_list_count(traf->sampleGroups) : 0;
	for (i=0; i<count; i++) {
		GF_SampleGroupBox *sg;
		u32 first_sample;
		Bool rap_type = GF_FALSE;
		sg = (GF_SampleGroupBox*)gf_list_get(traf->sampleGroups, i);

		switch (sg->grouping_type) {
		case GF_ISOM_SAMPLE_GROUP_RAP:
		case GF_ISOM_SAMPLE_GROUP_SYNC:
			rap_type = GF_TRUE;
			break;
		case GF_ISOM_SAMPLE_GROUP_ROLL:
			break;
		default:
			continue;
		}
		/*first entry is SAP*/
		first_sample = 1;
		for (j=0; j<sg->entry_count; j++) {
			if (! sg->sample_entries[j].group_description_index) {
				first_sample += sg->sample_entries[j].sample_count;
				continue;
			}
			if (!j) {
				*starts_with_sap = GF_TRUE;
				sap_sample_num = 0;
			}
			if (!sap_sample_num || (sap_sample_num>first_sample)) {
				sap_type = rap_type ? 3 : 4;
				sap_sample_num = first_sample;
			}
			break;
		}
	}
	//compute earliest cts in segment
	earliest_cts = get_earliest_cts_following(traf, 0, 0);

	/*then browse all samples, looking for SYNC flag or sap_sample_num*/
	cur_sample = 1;
	delta = 0;
	i=0;
	while ((trun = (GF_TrackFragmentRunBox*)gf_list_enum(traf->TrackRuns, &i))) {
		if (trun->flags & GF_ISOM_TRUN_FIRST_FLAG) {
			if (GF_ISOM_GET_FRAG_SYNC(trun->flags)) {
				ent = &trun->samples[0];
				s32 earliest_at_or_after_sap = get_earliest_cts_following(traf, i-1, 0);
				*sap_delta = (u32) (earliest_at_or_after_sap - earliest_cts);

				*starts_with_sap = first;
				sap_type = ent->SAP_type;
				return sap_type;
			}
		}
		for (j=0; j<trun->nb_samples; j++) {
			ent = &trun->samples[j];

			if (GF_ISOM_GET_FRAG_SYNC(ent->flags)) {
				s32 earliest_at_or_after_sap = get_earliest_cts_following(traf, i-1, j);
				*sap_delta = (u32) (earliest_at_or_after_sap - earliest_cts);
				*starts_with_sap = first;
				sap_type = ent->SAP_type;
				return sap_type;
			}
			/*we found our roll or rap sample*/
			if (cur_sample==sap_sample_num) {
				*sap_delta = (u32) (delta + ent->CTS_Offset - earliest_cts);
				return sap_type;
			}
			delta += ent->Duration;
			first = GF_FALSE;
			cur_sample++;
		}
	}
	/*not found*/
	return 0;
}

u32 moof_get_duration(GF_MovieFragmentBox *moof, GF_ISOTrackID refTrackID)
{
	u32 i, j, duration;
	GF_TrackFragmentBox *traf = NULL;
	GF_TrackFragmentRunBox *trun;
	for (i=0; i<gf_list_count(moof->TrackList); i++) {
		traf = (GF_TrackFragmentBox*)gf_list_get(moof->TrackList, i);
		if (traf->tfhd->trackID==refTrackID) break;
		traf=NULL;
	}
	if (!traf) return 0;

	duration = 0;
	i=0;
	while ((trun = (GF_TrackFragmentRunBox*)gf_list_enum(traf->TrackRuns, &i))) {
		for (j=0; j<trun->nb_samples; j++) {
			GF_TrunEntry *ent = &trun->samples[j];
			if (ent->flags & GF_ISOM_TRAF_SAMPLE_DUR)
				duration += ent->Duration;
			else
				duration += traf->trex->def_sample_duration;
		}
	}
	return duration;
}

static u64 moof_get_earliest_cts(GF_MovieFragmentBox *moof, GF_ISOTrackID refTrackID)
{
	u32 i, j;
	u64 cts, duration;
	GF_TrackFragmentBox *traf=NULL;
	GF_TrackFragmentRunBox *trun;
	for (i=0; i<gf_list_count(moof->TrackList); i++) {
		traf = (GF_TrackFragmentBox*)gf_list_get(moof->TrackList, i);
		if (traf->tfhd->trackID==refTrackID) break;
		traf=NULL;
	}
	if (!traf) return 0;

	duration = 0;
	cts = (u64) -1;
	i=0;
	while ((trun = (GF_TrackFragmentRunBox*)gf_list_enum(traf->TrackRuns, &i))) {
		for (j=0; j<trun->nb_samples; j++) {
			GF_TrunEntry *ent = &trun->samples[j];
			if (duration + ent->CTS_Offset < cts)
				cts = duration + ent->CTS_Offset;
			duration += ent->Duration;
		}
	}
	return cts;
}


GF_Err gf_isom_write_compressed_box(GF_ISOFile *mov, GF_Box *root_box, u32 repl_type, GF_BitStream *bs, u32 *box_csize);

void flush_ref_samples(GF_ISOFile *movie, u64 *out_seg_size, Bool use_seg_marker)
{
	u32 i=0;
	u32 traf_count = movie->in_sidx_write ? 0 : gf_list_count(movie->moof->TrackList);
	for (i=0; i<traf_count; i++) {
		GF_TrackFragmentBox *traf = gf_list_get(movie->moof->TrackList, i);
		u32 j, run_count = gf_list_count(traf->TrackRuns);
		if (!run_count) continue;
		for (j=0; j<run_count; j++) {
			GF_TrackFragmentRunBox *trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, j);
			u32 s_count = gf_list_count(trun->sample_refs);
			while (s_count) {
				if (!use_seg_marker && movie->on_last_block_start && (i+1==traf_count) && (j+1==run_count) && (s_count==1)) {
					movie->on_last_block_start(movie->on_block_out_usr_data);
				}
				GF_TrafSampleRef *sref = gf_list_pop_front(trun->sample_refs);
				movie->on_block_out(movie->on_block_out_usr_data, sref->data, sref->len, sref->ref, sref->ref_offset);
				if (out_seg_size) *out_seg_size += sref->len;
				if (!sref->ref) gf_free(sref->data);
				gf_free(sref);
				s_count--;
			}
		}
	}
}

GF_Err gf_bs_grow(GF_BitStream *bs, u32 addSize);

static GF_Err StoreFragment(GF_ISOFile *movie, Bool load_mdat_only, s32 data_offset_diff, u32 *moof_size, Bool reassign_bs)
{
	GF_Err e;
	u64 moof_start, pos, trun_ref_size=0;
	u32 size, i, s_count, mdat_size;
	s32 offset;
	u8 *buffer;
	GF_TrackFragmentBox *traf;
	GF_TrackFragmentRunBox *trun;
	GF_BitStream *bs, *bs_orig;
	if (!movie->moof) return GF_OK;

	bs = movie->editFileMap->bs;
	if (!movie->moof_first) {
		load_mdat_only = GF_FALSE;
		movie->force_moof_base_offset = GF_FALSE;
	}

	mdat_size = 0;
	//1 - flush all caches
	i=0;
	while ((traf = (GF_TrackFragmentBox*)gf_list_enum(movie->moof->TrackList, &i))) {
		u32 j, nb_written, last_gid, cur_sample_idx;
		/*do not write empty senc*/
		if (traf->sample_encryption && !gf_list_count(traf->sample_encryption->samp_aux_info)) {
			gf_list_del_item(traf->child_boxes, traf->sample_encryption);
			gf_isom_box_del((GF_Box *) traf->sample_encryption);
			traf->sample_encryption = NULL;
			/*remove saiz and saio (todo, check if other saiz/saio types are used*/
			for (j=0; j<gf_list_count(traf->sai_sizes); j++) {
				GF_SampleAuxiliaryInfoSizeBox *saiz = gf_list_get(traf->sai_sizes, j);
				switch (saiz->aux_info_type) {
				case GF_ISOM_CENC_SCHEME:
				case GF_ISOM_CBC_SCHEME:
				case GF_ISOM_CENS_SCHEME:
				case GF_ISOM_CBCS_SCHEME:
				case 0:
					gf_list_rem(traf->sai_sizes, j);
					gf_list_del_item(traf->child_boxes, saiz);
					gf_isom_box_del((GF_Box *)saiz);
					j--;
					break;
				}
			}
			for (j=0; j<gf_list_count(traf->sai_offsets); j++) {
				GF_SampleAuxiliaryInfoOffsetBox *saio = gf_list_get(traf->sai_offsets, j);
				switch (saio->aux_info_type) {
				case GF_ISOM_CENC_SCHEME:
				case GF_ISOM_CBC_SCHEME:
				case GF_ISOM_CENS_SCHEME:
				case GF_ISOM_CBCS_SCHEME:
				case 0:
					gf_list_rem(traf->sai_offsets, j);
					gf_list_del_item(traf->child_boxes, saio);
					gf_isom_box_del((GF_Box *)saio);
					j--;
					break;
				}
			}
		}
		trun_ref_size += traf->trun_ref_size;
		if (!traf->DataCache) continue;

		s_count = gf_list_count(traf->TrackRuns);
		if (!s_count) continue;

		//store all cached truns - there may be more than one when using sample interleaving in truns
		nb_written = 0;
		last_gid = 0;
		cur_sample_idx = 0;
		while (nb_written<s_count) {
			u32 min_next_gid = 0xFFFFFFFF;

			for (j=0; j<s_count; j++) {
				trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, j);
				//done
				if (!trun->cache || !trun->sample_count) continue;

				if (!traf->use_sample_interleave || (last_gid!=trun->interleave_id)) {
					if (trun->interleave_id < min_next_gid)
						min_next_gid = trun->interleave_id;
					continue;
				}

				//update offset
				trun->data_offset = (u32) (gf_bs_get_position(bs) - movie->moof->fragment_offset - 8);
				//write cache
				gf_bs_get_content(trun->cache, &buffer, &size);
				gf_bs_write_data(bs, buffer, size);
				gf_bs_del(trun->cache);
				gf_free(buffer);
				trun->cache = NULL;
				trun->first_sample_idx = cur_sample_idx;
				cur_sample_idx += trun->sample_count;

				nb_written++;
			}
			last_gid = min_next_gid;
		}

		traf->DataCache=0;
	}

	if (load_mdat_only) {
		pos = trun_ref_size ? (trun_ref_size+8) : gf_bs_get_position(bs);
		if (movie->moof->fragment_offset > pos)
			return GF_CORRUPTED_DATA;

		//we assume we never write large MDATs in fragment mode which should always be true
		movie->moof->mdat_size = (u32) (pos - movie->moof->fragment_offset);

		if (trun_ref_size) {
			gf_bs_seek(movie->editFileMap->bs, 0);
			return GF_OK;
		}
		if (movie->segment_bs) {
			e = gf_bs_seek(bs, 0);
			if (e) return e;
			/*write mdat size*/
			gf_bs_write_u32(bs, (u32) movie->moof->mdat_size);
			/*and get internal buffer*/
			e = gf_bs_seek(bs, movie->moof->mdat_size);
			if (e) return e;
			gf_bs_get_content(bs, &movie->moof->mdat, &movie->moof->mdat_size);

			gf_bs_del(bs);
			movie->editFileMap->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		} else {
			u64 frag_offset = movie->segment_start;
			e = gf_bs_seek(bs, frag_offset);
			if (e) return e;
			/*write mdat size*/
			gf_bs_write_u32(bs, (u32) movie->moof->mdat_size);

			movie->moof->mdat = (char*)gf_malloc(sizeof(char) * movie->moof->mdat_size);
			if (!movie->moof->mdat) return GF_OUT_OF_MEM;

			e = gf_bs_seek(bs, frag_offset);
			if (e) return e;
			gf_bs_read_data(bs, movie->moof->mdat, movie->moof->mdat_size);

			e = gf_bs_seek(bs, frag_offset);
			if (e) return e;
			gf_bs_truncate(bs);
		}

		return GF_OK;
	}

	moof_start = gf_bs_get_position(bs);

	if (movie->moof->ntp) {
		moof_start += 8*4;
	}

	//2- update MOOF MDAT header
	if (!movie->moof->mdat && !trun_ref_size) {
		e = gf_bs_seek(bs, movie->moof->fragment_offset);
		if (e) return e;
		//we assume we never write large MDATs in fragment mode which should always be true
		mdat_size = (u32) (moof_start - movie->moof->fragment_offset);
		gf_bs_write_u32(bs, (u32) mdat_size);
		gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_MDAT);
		e = gf_bs_seek(bs, moof_start);
		if (e) return e;
	}

	/*estimate moof size and shift trun offsets*/
#ifndef USE_BASE_DATA_OFFSET
	offset = 0;
	if (movie->use_segments || movie->force_moof_base_offset) {
		e = gf_isom_box_size((GF_Box *) movie->moof);
		if (e) return e;
		offset = (s32) movie->moof->size;
		/*mdat size & type*/
		offset += 8;
		update_trun_offsets(movie, offset);
	}
#endif

	//3- clean our traf's
	i=0;
	while ((traf = (GF_TrackFragmentBox*) gf_list_enum(movie->moof->TrackList, &i))) {
		//compute default settings for the TRAF
		ComputeFragmentDefaults(traf);
		//updates all trun and set all flags, INCLUDING TRAF FLAGS (durations, ...)
		s_count = UpdateRuns(movie, traf);
		//empty fragment destroy it
		if (!traf->tfhd->EmptyDuration && !s_count) {
			i--;
			gf_list_rem(movie->moof->TrackList, i);
			gf_isom_box_del_parent(&movie->moof->child_boxes, (GF_Box *) traf);
			continue;
		}
	}

	buffer = NULL;
	/*rewind bitstream and load mdat in memory */
	if (movie->moof_first && !movie->moof->mdat && !trun_ref_size) {
		buffer = (char*)gf_malloc(sizeof(char)*mdat_size);
		if (!buffer) return GF_OUT_OF_MEM;
		e = gf_bs_seek(bs, movie->moof->fragment_offset);
		if (e) return e;
		gf_bs_read_data(bs, buffer, mdat_size);
		/*back to mdat start and erase with moov*/
		e = gf_bs_seek(bs, movie->moof->fragment_offset);
		if (e) return e;
		gf_bs_truncate(bs);
	}

	//4- Write moof
	e = gf_isom_box_size((GF_Box *) movie->moof);
	if (e) return e;
	/*moof first, update traf headers - THIS WILL IMPACT THE MOOF SIZE IF WE
	DECIDE NOT TO USE THE DATA-OFFSET FLAG*/
	if (movie->moof_first
#ifndef USE_BASE_DATA_OFFSET
	        && !(movie->use_segments || movie->force_moof_base_offset)
#endif
	   ) {
		i=0;
		while ((traf = (GF_TrackFragmentBox*)gf_list_enum(movie->moof->TrackList, &i))) {
			/*offset increases by moof size*/
			traf->tfhd->base_data_offset += movie->moof->size;
			traf->tfhd->base_data_offset += data_offset_diff;
			if (movie->on_block_out) {
				traf->tfhd->base_data_offset += movie->fragmented_file_pos;
			}
		}
	}
#ifndef USE_BASE_DATA_OFFSET
	else if (movie->use_segments || movie->force_moof_base_offset) {
		if (offset != (movie->moof->size+8)) {
			offset = (s32) (movie->moof->size + 8 - offset);
			update_trun_offsets(movie, offset);
			e = gf_isom_box_size((GF_Box *) movie->moof);
			if (e) return e;
		}
	}
#endif

	if (!movie->moof_first && !movie->force_moof_base_offset) {
		i=0;
		while ((traf = (GF_TrackFragmentBox*)gf_list_enum(movie->moof->TrackList, &i))) {
			/*offset increases by moof size*/
			if (movie->on_block_out) {
				traf->tfhd->base_data_offset += movie->fragmented_file_pos;
			}
		}
	}

	bs_orig = bs;
	if (reassign_bs && movie->on_block_out) {
		bs = gf_bs_new_cbk(isom_on_block_out, movie, movie->on_block_out_block_size);
	}

	if (trun_ref_size && movie->in_sidx_write) {
		bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	}

	if (movie->moof->ntp) {
		gf_bs_write_u32(bs, 8*4);
		gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_PRFT );
		gf_bs_write_u8(bs, 1);
		gf_bs_write_u24(bs, 0);
		gf_bs_write_u32(bs, movie->moof->reference_track_ID);
		gf_bs_write_u64(bs, movie->moof->ntp);
		gf_bs_write_u64(bs, movie->moof->timestamp);
	}
	if (movie->moof->emsgs) {
		while (1) {
			GF_Box *emsg = gf_list_pop_front(movie->moof->emsgs);
			if (!emsg) break;
			gf_isom_box_size(emsg);
			gf_isom_box_write(emsg, bs);
			gf_isom_box_del(emsg);
		}
		gf_list_del(movie->moof->emsgs);
		movie->moof->emsgs = NULL;
	}

	if (moof_size) *moof_size = (u32) movie->moof->size;

	pos = gf_bs_get_position(bs);
	//graw buffer to hold moof, speeds up writes
	gf_bs_grow(bs, (u32) movie->moof->size + (trun_ref_size ? 8 : 0));

	i=0;
	while ((traf = (GF_TrackFragmentBox*)gf_list_enum(movie->moof->TrackList, &i))) {
		traf->moof_start_in_bs = pos;
	}

	/*we don't want to dispatch any block until done writing the moof*/
	if (movie->on_block_out)
		gf_bs_prevent_dispatch(bs, GF_TRUE);

	if (movie->compress_mode>GF_ISOM_COMP_MOOV) {
		e = gf_isom_write_compressed_box(movie, (GF_Box *) movie->moof, GF_4CC('!', 'm', 'o', 'f'), bs, moof_size);
	} else {
		e = gf_isom_box_write((GF_Box *) movie->moof, bs);
	}

	if (trun_ref_size) {
		gf_bs_write_u32(bs, movie->moof->mdat_size);
		gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_MDAT);
	}

	if (movie->on_block_out)
		gf_bs_prevent_dispatch(bs, GF_FALSE);

	if (e) return e;

	if (trun_ref_size) {
		flush_ref_samples(movie, NULL, GF_FALSE);
	} else {
		if (movie->on_last_block_start && !gf_list_count(movie->moof_list))
			movie->on_last_block_start(movie->on_block_out_usr_data);

		//rewrite mdat after moof
		if (movie->moof->mdat) {
			gf_bs_write_data(bs, movie->moof->mdat, movie->moof->mdat_size);
			gf_free(movie->moof->mdat);
			movie->moof->mdat = NULL;
		} else if (buffer) {
			gf_bs_write_data(bs, buffer, mdat_size);
			gf_free(buffer);
		}
	}

	if (trun_ref_size && movie->in_sidx_write) {
		gf_bs_get_content(bs, &movie->moof->moof_data, &movie->moof->moof_data_len);
		gf_bs_del(bs);
		movie->fragmented_file_pos += movie->moof->moof_data_len + trun_ref_size;
		movie->moof->trun_ref_size = (u32) trun_ref_size;
	}
	else if (bs != bs_orig) {
		u64 frag_size = gf_bs_get_position(bs);
		gf_bs_del(bs);
		movie->fragmented_file_pos += frag_size + trun_ref_size;
		gf_bs_seek(bs_orig, 0);
		gf_bs_truncate(bs_orig);
	}
	else if (movie->on_block_out) {
		u64 frag_size = gf_bs_get_position(bs);
		movie->fragmented_file_pos += frag_size + trun_ref_size;
	}

	if (!movie->use_segments) {
		//remove from moof list (may happen in regular fragmentation when single traf per moof is used)
		gf_list_del_item(movie->moof_list, movie->moof);
		gf_isom_box_del((GF_Box *) movie->moof);
		movie->moof = NULL;
	}
	return GF_OK;
}

static GF_Err sidx_rewrite(GF_SegmentIndexBox *sidx, GF_BitStream *bs, u64 start_pos, GF_SubsegmentIndexBox *ssix)
{
	GF_Err e = GF_OK;
	u64 pos = gf_bs_get_position(bs);
	if (ssix) {
		e = gf_isom_box_size((GF_Box *)ssix);
		sidx->first_offset = ssix->size;
	}
	/*write sidx*/
	gf_bs_seek(bs, start_pos);
	if (!e) e = gf_isom_box_write((GF_Box *) sidx, bs);
	if (!e && ssix) {
		e = gf_isom_box_write((GF_Box *) ssix, bs);
	}
	gf_bs_seek(bs, pos);
	return e;
}

GF_Err gf_isom_allocate_sidx(GF_ISOFile *movie, s32 subsegs_per_sidx, Bool daisy_chain_sidx, u32 nb_segs, u32 *frags_per_segment, u32 *start_range, u32 *end_range, Bool use_ssix)
{
	GF_BitStream *bs;
	GF_Err e;
	u32 i;

	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;
	if (movie->root_sidx) return GF_BAD_PARAM;
	if (movie->root_ssix) return GF_BAD_PARAM;
	if (movie->moof) return GF_BAD_PARAM;
	if (gf_list_count(movie->moof_list)) return GF_BAD_PARAM;

	movie->root_sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
	if (!movie->root_sidx) return GF_OUT_OF_MEM;
	/*we don't write anything between sidx and following moov*/
	movie->root_sidx->first_offset = 0;

	/*for now we only store one ref per subsegment and don't support daisy-chaining*/
	movie->root_sidx->nb_refs = nb_segs;

	if (use_ssix) {
		movie->root_ssix = (GF_SubsegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SSIX);
		movie->root_ssix->subsegment_count = nb_segs;
		movie->root_ssix->subsegment_alloc = movie->root_ssix->subsegment_count;
	}

	//dynamic mode
	if (!nb_segs) {
		movie->dyn_root_sidx = GF_TRUE;
		return GF_OK;
	}

	movie->root_sidx->refs = (GF_SIDXReference*)gf_malloc(sizeof(GF_SIDXReference) * movie->root_sidx->nb_refs);
	if (!movie->root_sidx->refs) return GF_OUT_OF_MEM;
	memset(movie->root_sidx->refs, 0, sizeof(GF_SIDXReference) * movie->root_sidx->nb_refs);

	movie->root_sidx_index = 0;

	if (use_ssix) {
		movie->root_ssix->subsegments = gf_malloc(sizeof(GF_SubsegmentInfo) * nb_segs);
		if (!movie->root_ssix->subsegments) return GF_OUT_OF_MEM;
		for (i=0; i<nb_segs; i++) {
			movie->root_ssix->subsegments[i].range_count = 2;
			movie->root_ssix->subsegments[i].ranges = gf_malloc(sizeof(GF_SubsegmentRangeInfo)*2);
			if (!movie->root_ssix->subsegments[i].ranges) return GF_OUT_OF_MEM;
			movie->root_ssix->subsegments[i].ranges[0].level = 0;
			movie->root_ssix->subsegments[i].ranges[0].range_size = 0;
			movie->root_ssix->subsegments[i].ranges[1].level = 1;
			movie->root_ssix->subsegments[i].ranges[1].range_size = 0;
		}
	}
	
	/*remember start of sidx*/
	movie->root_sidx_offset = gf_bs_get_position(movie->editFileMap->bs);

	bs = movie->editFileMap->bs;

	e = gf_isom_box_size((GF_Box *) movie->root_sidx);
	if (e) return e;
	e = gf_isom_box_write((GF_Box *) movie->root_sidx, bs);
	if (e) return e;

	if (movie->root_ssix) {
		e = gf_isom_box_size((GF_Box *) movie->root_ssix);
		if (e) return e;
		e = gf_isom_box_write((GF_Box *) movie->root_ssix, bs);
		if (e) return e;
	}

	//include ssix in index range - spec is not clear whether this is forbidden
	if (start_range) *start_range = (u32) movie->root_sidx_offset;
	if (end_range) *end_range = (u32) gf_bs_get_position(bs)-1;

	return GF_OK;
}


static GF_Err gf_isom_write_styp(GF_ISOFile *movie, Bool last_segment)
{
	/*write STYP if we write to a different file or if we write the last segment*/
	if (movie->use_segments && !movie->append_segment && !movie->segment_start && movie->write_styp) {
		GF_Err e;

		/*modify brands STYP*/
		if (movie->write_styp==1) {
			/*"msix" brand: this is a DASH Initialization Segment*/
			gf_isom_modify_alternate_brand(movie, GF_ISOM_BRAND_MSIX, GF_TRUE);
			if (last_segment) {
				/*"lmsg" brand: this is the last DASH Segment*/
				gf_isom_modify_alternate_brand(movie, GF_ISOM_BRAND_LMSG, GF_TRUE);
			}
		}
		movie->brand->type = GF_ISOM_BOX_TYPE_STYP;
		e = gf_isom_box_size((GF_Box *) movie->brand);
		if (e) return e;
		e = gf_isom_box_write((GF_Box *) movie->brand, movie->editFileMap->bs);
		if (e) return e;

		movie->write_styp = 0;
	}

	if (movie->emsgs) {
		while (1) {
			GF_Box *b = gf_list_pop_front(movie->emsgs);
			if (!b) break;
			gf_isom_box_size(b);
			gf_isom_box_write(b, movie->editFileMap->bs);
			gf_isom_box_del(b);
		}
		gf_list_del(movie->emsgs);
		movie->emsgs = NULL;
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_flush_fragments(GF_ISOFile *movie, Bool last_segment)
{
	GF_BitStream *temp_bs = NULL, *orig_bs;
	GF_Err e;

	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	/*flush our fragment (store in mem)*/
	if (movie->moof) {
		e = StoreFragment(movie, GF_TRUE, 0, NULL, GF_FALSE);
		if (e) return e;
	}

	if (movie->segment_bs) {
		temp_bs = movie->editFileMap->bs;
		movie->editFileMap->bs = movie->segment_bs;
	}

	if (movie->moof_first) {
		gf_bs_seek(movie->editFileMap->bs, movie->segment_start);
		gf_bs_truncate(movie->editFileMap->bs);
	}

	orig_bs = movie->editFileMap->bs;
	if (movie->on_block_out) {
		if (!movie->block_buffer) movie->block_buffer_size = movie->on_block_out_block_size;
		movie->editFileMap->bs = gf_bs_new_cbk_buffer(isom_on_block_out, movie, movie->block_buffer, movie->block_buffer_size);
	}

	/*write styp to file if needed*/
	e = gf_isom_write_styp(movie, last_segment);
	if (e) goto exit;

	/*write all pending fragments to file*/
	while (gf_list_count(movie->moof_list)) {
		s32 offset_diff;
		u32 moof_size;

		movie->moof = (GF_MovieFragmentBox*)gf_list_get(movie->moof_list, 0);
		gf_list_rem(movie->moof_list, 0);

		offset_diff = (s32) (gf_bs_get_position(movie->editFileMap->bs) - movie->moof->fragment_offset);
		movie->moof->fragment_offset = gf_bs_get_position(movie->editFileMap->bs);

		e = StoreFragment(movie, GF_FALSE, offset_diff, &moof_size, GF_FALSE);
		if (e) goto exit;

		gf_isom_box_del((GF_Box *) movie->moof);
		movie->moof = NULL;
	}

	/*append mode: store fragment at the end of the regular movie bitstream, and delete the temp bitstream*/
	if (movie->append_segment) {
		char bloc[1024];
		u32 seg_size = (u32) gf_bs_get_size(movie->editFileMap->bs);
		gf_bs_seek(movie->editFileMap->bs, 0);
		while (seg_size) {
			u32 size = gf_bs_read_data(movie->editFileMap->bs, bloc, (seg_size>1024) ? 1024 : seg_size);
			gf_bs_write_data(movie->movieFileMap->bs, bloc, size);
			seg_size -= size;
		}
		gf_isom_datamap_flush(movie->movieFileMap);

		gf_isom_datamap_del(movie->editFileMap);
		movie->editFileMap = gf_isom_fdm_new_temp(NULL);
	} else {
		gf_isom_datamap_flush(movie->editFileMap);
	}
	movie->segment_start = gf_bs_get_position(movie->editFileMap->bs);

	if (temp_bs) {
		movie->segment_bs = movie->editFileMap->bs;
		movie->editFileMap->bs = temp_bs;
	}

	if (orig_bs != movie->editFileMap->bs) {
		u32 tmpsize;
		if (!movie->moof_first) {
			gf_bs_transfer(movie->editFileMap->bs, orig_bs, GF_TRUE);
			gf_bs_seek(orig_bs, 0);
		}
		gf_bs_get_content_no_truncate(movie->editFileMap->bs, &movie->block_buffer, &tmpsize, &movie->block_buffer_size);
		gf_bs_del(movie->editFileMap->bs);
		movie->editFileMap->bs = orig_bs;
		//we are dispatching through callbacks, the movie segment start is always 0
		movie->segment_start = 0;
	}
exit:
	return e;
}

typedef struct
{
	GF_SegmentIndexBox *sidx;
	u64 start_offset, end_offset;
} SIDXEntry;

static u64 get_presentation_time(u64 media_time, s32 ts_shift)
{
	if ((ts_shift<0) && (media_time < -ts_shift)) {
		media_time = 0;
	} else {
		media_time += ts_shift;
	}
	return media_time ;
}


#if 0 //unused
/*! gets name of current segment (or last segment if called between close_segment and start_segment)
\param isom_file the target ISO file
\return associated file name of the segment
*/
GF_EXPORT
const char *gf_isom_get_segment_name(GF_ISOFile *movie)
{
	if (!movie) return NULL;
	if (movie->append_segment) return movie->movieFileMap->szName;
	return movie->editFileMap->szName;
}
#endif

static void compute_seg_size(GF_ISOFile *movie, u64 *out_seg_size)
{
	u64 final_size = 0;
	if (out_seg_size) {
		if (movie->append_segment) {
			final_size = gf_bs_get_position(movie->movieFileMap->bs);
			final_size -= movie->segment_start;
		} else if (movie->editFileMap) {
			final_size = gf_bs_get_position(movie->editFileMap->bs);
		}
		*out_seg_size = final_size;
	}
}

static u32 moof_get_first_sap_end(GF_MovieFragmentBox *moof)
{
	u32 i, count = gf_list_count(moof->TrackList);
	for (i=0; i<count; i++) {
		u32 j, nb_trun;
		GF_TrackFragmentBox *traf = gf_list_get(moof->TrackList, i);
		u32 base_offset = (u32) traf->tfhd->base_data_offset;

		nb_trun = gf_list_count(traf->TrackRuns);
		for (j=0; j<nb_trun; j++) {
			u32 k;
			GF_TrackFragmentRunBox *trun = gf_list_get(traf->TrackRuns, j);
			u32 offset = base_offset + trun->data_offset;
			for (k=0; k<trun->nb_samples; k++) {
				GF_TrunEntry *ent = &trun->samples[k];
				if (ent->SAP_type) return offset + ent->size;

				offset += ent->size;
			}
		}
	}
	return 0;
}

static u64 estimate_next_moof_earliest_presentation_time(u64 ref_track_decode_time, s32 ts_shift, u32 refTrackID, GF_ISOFile *movie)
{
	u32 i, j, nb_aus, nb_ctso, nb_moof;
	u64 duration;
	GF_TrunEntry *ent;
	GF_TrackFragmentBox *traf=NULL;
	GF_TrackFragmentRunBox *trun;
	u32 timescale;
	u64 min_next_cts = -1;

	GF_MovieFragmentBox *moof = gf_list_get(movie->moof_list, 0);

	for (i=0; i<gf_list_count(moof->TrackList); i++) {
		traf = (GF_TrackFragmentBox*)gf_list_get(moof->TrackList, i);
		if (traf->tfhd->trackID==refTrackID) break;
		traf = NULL;
	}
	//no ref track, nothing to estimate
	if (!traf) return -1;
	timescale = traf->trex->track->Media->mediaHeader->timeScale;

	nb_aus = 0;
	duration = 0;
	nb_ctso = 0;
	nb_moof = 0;

	while ((moof = (GF_MovieFragmentBox*)gf_list_enum(movie->moof_list, &nb_moof))) {

		for (i=0; i<gf_list_count(moof->TrackList); i++) {
			traf = (GF_TrackFragmentBox*)gf_list_get(moof->TrackList, i);
			if (traf->tfhd->trackID==refTrackID) break;
			traf = NULL;
		}
		if (!traf) continue;

		i=0;
		while ((trun = (GF_TrackFragmentRunBox*)gf_list_enum(traf->TrackRuns, &i))) {
			for (j=0; j<trun->nb_samples; j++) {
				ent = &trun->samples[j];
				if (nb_aus + 1 + movie->sidx_pts_store_count > movie->sidx_pts_store_alloc) {
					movie->sidx_pts_store_alloc = movie->sidx_pts_store_count+nb_aus+1;
					movie->sidx_pts_store = gf_realloc(movie->sidx_pts_store, sizeof(u64) * movie->sidx_pts_store_alloc);
					movie->sidx_pts_next_store = gf_realloc(movie->sidx_pts_next_store, sizeof(u64) * movie->sidx_pts_store_alloc);
				}
				//get PTS for this AU, push to regular list
				movie->sidx_pts_store[movie->sidx_pts_store_count + nb_aus] = get_presentation_time( ref_track_decode_time + duration + ent->CTS_Offset, ts_shift);
				//get PTS for this AU shifted by its presentation duration, push to shifted list
				movie->sidx_pts_next_store[movie->sidx_pts_store_count + nb_aus] = get_presentation_time( ref_track_decode_time + duration + ent->CTS_Offset + ent->Duration, ts_shift);
				duration += ent->Duration;
				if (ent->CTS_Offset)
					nb_ctso++;

				nb_aus++;
			}
		}
	}

	movie->sidx_pts_store_count += nb_aus;

	//no AUs, nothing to estimate
	if (!nb_aus) {
		movie->sidx_pts_store_count = 0;
		return -1;
	}
	//no cts offset, assume earliest PTS in next segment is last PTS in this segment + duration
	if (!nb_ctso) {
		min_next_cts = movie->sidx_pts_next_store[movie->sidx_pts_store_count - 1];
		movie->sidx_pts_store_count = 0;
		return min_next_cts;
	}

	//look for all shifted PTS of this segment in the regular list. If found in the shifted list, the AU is in this segment
	//remove from both list
	for (i=0; i<movie->sidx_pts_store_count; i++) {
		for (j=i; j<movie->sidx_pts_store_count; j++) {
			/*

 			if (movie->sidx_pts_next_store[i] == movie->sidx_pts_store[j]) {
 			
			take care of misaligned timescale eg 24fps but 10000 timescale), we may not find exactly
			the same sample - if diff below N ms consider it a match
			not doing so would accumulate PTSs in the list, slowing down the muxing

			using N=1ms strict would not be enough to take into account sources with approximate timing - cf issue #2436
			we use N=2ms max to handle sources with high jitter in cts
			*/
			s64 diff = movie->sidx_pts_next_store[i];
			diff -= (s64) movie->sidx_pts_store[j];
			if (diff && (timescale>1000)) {
				if (ABS(diff) * 1000 < 2 * timescale)
					diff = 0;
			}
			if (diff==0) {
				if (movie->sidx_pts_store_count >= i + 1)
					memmove(&movie->sidx_pts_next_store[i], &movie->sidx_pts_next_store[i+1], sizeof(u64) * (movie->sidx_pts_store_count - i - 1) );
				if (movie->sidx_pts_store_count >= j + 1)
					memmove(&movie->sidx_pts_store[j], &movie->sidx_pts_store[j+1], sizeof(u64) * (movie->sidx_pts_store_count - j - 1) );
				movie->sidx_pts_store_count--;
				i--;
				break;
			}
		}
	}
	//the shifted list contain all AUs not yet in this segment, keep the smallest to compute the earliest PTS in next seg
	//note that we assume the durations were correctly set
	for (i=0; i<movie->sidx_pts_store_count; i++) {
		if (min_next_cts > movie->sidx_pts_next_store[i])
			min_next_cts = movie->sidx_pts_next_store[i];
	}
	return min_next_cts;
}


GF_EXPORT
GF_Err gf_isom_close_segment(GF_ISOFile *movie, s32 subsegments_per_sidx, GF_ISOTrackID referenceTrackID, u64 ref_track_decode_time, s32 ts_shift, u64 ref_track_next_cts, Bool daisy_chain_sidx, Bool use_ssix, Bool last_segment, Bool close_segment_handle, u32 segment_marker_4cc, u64 *index_start_range, u64 *index_end_range, u64 *out_seg_size)
{
	GF_SegmentIndexBox *sidx=NULL;
	GF_SegmentIndexBox *root_sidx=NULL;
	GF_SubsegmentIndexBox *ssix=NULL;
	GF_List *daisy_sidx = NULL;
	GF_List *defer_moofs = NULL;
	GF_BitStream *orig_bs;
	u64 sidx_start, sidx_end;
	Bool first_frag_in_subseg;
	Bool no_sidx = GF_FALSE;
	u32 count, cur_idx, cur_dur, sidx_dur, sidx_idx, idx_offset, frag_count;
	u64 last_top_box_pos, root_prev_offset, local_sidx_start, local_sidx_end, prev_earliest_cts, next_earliest_cts;
	GF_TrackBox *trak = NULL;
	GF_Err e;
	/*number of subsegment in this segment (eg nb references in the first SIDX found)*/
	u32 nb_subsegs=0;
	/*number of subsegment per sidx (eg number of references of any sub-SIDX*/
	u32 subseg_per_sidx;
	/*number of fragments per subsegment*/
	u32 frags_per_subseg;
	/*number of fragments per subsidx*/
	u32 frags_per_subsidx;

	sidx_start = sidx_end = 0;

	if (index_start_range) *index_start_range = 0;
	if (index_end_range) *index_end_range = 0;

	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	count = gf_list_count(movie->moov->mvex->TrackExList);
	if (!count) return GF_BAD_PARAM;

	/*store fragment*/
	if (movie->moof) {
		e = StoreFragment(movie, GF_TRUE, 0, NULL, GF_FALSE);
		if (e) return e;
	}
	/*restore final bitstream*/
	if (movie->segment_bs) {
		gf_bs_del(movie->editFileMap->bs);
		movie->editFileMap->bs = movie->segment_bs;
		movie->segment_bs = NULL;
	}

	count = gf_list_count(movie->moof_list);
	if (!count) {
		/*append segment marker box*/
		if (segment_marker_4cc) {
			if (movie->append_segment) {
				gf_bs_write_u32(movie->movieFileMap->bs, 8);	//write size field
				gf_bs_write_u32(movie->movieFileMap->bs, segment_marker_4cc); //write box type field
			} else {
				gf_bs_write_u32(movie->editFileMap->bs, 8);	//write size field
				gf_bs_write_u32(movie->editFileMap->bs, segment_marker_4cc); //write box type field
			}
		}

		compute_seg_size(movie, out_seg_size);

		if (close_segment_handle) {
			gf_isom_datamap_del(movie->editFileMap);
			movie->editFileMap = NULL;
		}

		return GF_OK;
	}

	gf_bs_seek(movie->editFileMap->bs, movie->segment_start);
	gf_bs_truncate(movie->editFileMap->bs);

	idx_offset = 0;

	if (referenceTrackID) {
		trak = gf_isom_get_track_from_id(movie->moov, referenceTrackID);
		if (!trak) return GF_BAD_PARAM;
	}

	if (subsegments_per_sidx < 0) {
		referenceTrackID = 0;
		subsegments_per_sidx = 0;
	}
	if (!subsegments_per_sidx && !referenceTrackID) {
		no_sidx = GF_TRUE;
	}

	orig_bs = movie->editFileMap->bs;
	if (movie->on_block_out) {
		if (!movie->block_buffer) movie->block_buffer_size = movie->on_block_out_block_size;
		movie->editFileMap->bs = gf_bs_new_cbk_buffer(isom_on_block_out, movie, movie->block_buffer, movie->block_buffer_size);
		if (referenceTrackID) gf_bs_prevent_dispatch(movie->editFileMap->bs, GF_TRUE);
	}

	e = gf_isom_write_styp(movie, last_segment);
	if (e) goto exit;

	frags_per_subseg = 0;
	subseg_per_sidx = 0;
	frags_per_subsidx = 0;

	prev_earliest_cts = 0;
	next_earliest_cts = 0;

	if (daisy_chain_sidx)
		daisy_sidx = gf_list_new();

	/*prepare SIDX: we write a blank SIDX box with the right number of entries, and will rewrite it later on*/
	if (referenceTrackID) {
		Bool is_root_sidx = GF_FALSE;

		prev_earliest_cts = get_presentation_time( ref_track_decode_time + moof_get_earliest_cts((GF_MovieFragmentBox*)gf_list_get(movie->moof_list, 0), referenceTrackID), ts_shift);

		//we don't trust ref_track_next_cts to be the earliest in the following segment
		next_earliest_cts = estimate_next_moof_earliest_presentation_time(ref_track_decode_time, ts_shift, referenceTrackID, movie);

		if (movie->root_sidx) {
			sidx = movie->root_sidx;
		} else {
			sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
			if (!sidx) return GF_OUT_OF_MEM;
			if (movie->force_sidx_v1)
				sidx->version = 1;
		}
		sidx->reference_ID = referenceTrackID;
		sidx->timescale = trak->Media->mediaHeader->timeScale;
		/*we don't write anything between sidx and following moov*/
		sidx->first_offset = 0;

		/*we allocated our sidx to have one ref per "segment" (eg per call to close_segment)*/
		if (movie->root_sidx) {
			if (!movie->root_sidx_index) {
				sidx->earliest_presentation_time = prev_earliest_cts;
			}
			nb_subsegs = 1;
			frags_per_subseg = count;
			frags_per_subsidx = count;
			subseg_per_sidx = 1;
			daisy_chain_sidx = GF_FALSE;

			idx_offset = movie->root_sidx_index;
			sidx_end = gf_bs_get_position(movie->editFileMap->bs);
		} else {
			sidx->earliest_presentation_time = prev_earliest_cts;

			/*if more subsegments requested than fragments available, make a single sidx*/
			if ((s32) count <= subsegments_per_sidx)
				subsegments_per_sidx = 0;

			if (daisy_chain_sidx && (subsegments_per_sidx<2))
				subsegments_per_sidx = 2;

			/*single SIDX, each fragment is a subsegment and we reference all subsegments*/
			if (!subsegments_per_sidx) {
				nb_subsegs = count;
				/*we consider that each fragment is a subsegment - this could be controled by another parameter*/
				frags_per_subseg = 1;
				frags_per_subsidx = count;
				subseg_per_sidx = count;

				sidx->nb_refs = nb_subsegs;
				daisy_chain_sidx = GF_FALSE;
			}
			/*daisy-chain SIDX: each SIDX describes a subsegment made of frags_per_subseg fragments plus next */
			else if (daisy_chain_sidx) {
				frags_per_subsidx = count/subsegments_per_sidx;
				if (frags_per_subsidx * subsegments_per_sidx < count) frags_per_subsidx++;

				nb_subsegs = subsegments_per_sidx;

				/*we consider that each fragment is a subsegment - this could be controled by another parameter*/
				frags_per_subseg = 1;
				subseg_per_sidx = frags_per_subsidx / frags_per_subseg;
				if (subseg_per_sidx * frags_per_subseg < frags_per_subsidx) subseg_per_sidx++;

				sidx->nb_refs = subseg_per_sidx + 1;
			}
			/*hierarchical SIDX*/
			else {
				frags_per_subsidx = count/subsegments_per_sidx;
				if (frags_per_subsidx * subsegments_per_sidx < count) frags_per_subsidx++;

				nb_subsegs = subsegments_per_sidx;

				/*we consider that each fragment is a subsegment - this could be controled by another parameter*/
				frags_per_subseg = 1;
				subseg_per_sidx = frags_per_subsidx / frags_per_subseg;
				if (subseg_per_sidx * frags_per_subseg < frags_per_subsidx) subseg_per_sidx++;

				sidx->nb_refs = nb_subsegs;
				is_root_sidx = GF_TRUE;
			}

			sidx->refs = (GF_SIDXReference*)gf_malloc(sizeof(GF_SIDXReference)*sidx->nb_refs);
			if (!sidx->refs) return GF_OUT_OF_MEM;
			memset(sidx->refs, 0, sizeof(GF_SIDXReference)*sidx->nb_refs);

			/*remember start of sidx*/
			sidx_start = gf_bs_get_position(movie->editFileMap->bs);

			e = gf_isom_box_size((GF_Box *) sidx);
			if (e) goto exit;
			e = gf_isom_box_write((GF_Box *) sidx, movie->editFileMap->bs);
			if (e) goto exit;

			if (use_ssix && !ssix && !movie->root_ssix) {
				u32 k;
				ssix = (GF_SubsegmentIndexBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SSIX);
				if (!ssix) return GF_OUT_OF_MEM;
				ssix->subsegments = gf_malloc(sizeof(GF_SubsegmentInfo) * sidx->nb_refs);
				if (!ssix->subsegments) return GF_OUT_OF_MEM;
				ssix->subsegment_count = sidx->nb_refs;
				ssix->subsegment_alloc = ssix->subsegment_count;
				for (k=0; k<sidx->nb_refs; k++) {
					GF_SubsegmentInfo *subs = &ssix->subsegments[k];
					subs->range_count = 2;
					subs->ranges = gf_malloc(sizeof(GF_SubsegmentRangeInfo)*2);
					if (!subs->ranges) return GF_OUT_OF_MEM;
					subs->ranges[0].level = 1;
					subs->ranges[1].level = 2;
					subs->ranges[0].range_size = subs->ranges[1].range_size = 0;
				}

				e = gf_isom_box_size((GF_Box *) ssix);
				if (e) return e;
				e = gf_isom_box_write((GF_Box *) ssix, movie->editFileMap->bs);
				if (e) return e;
			}

			sidx_end = gf_bs_get_position(movie->editFileMap->bs);

			if (daisy_sidx) {
				SIDXEntry *entry;
				GF_SAFEALLOC(entry, SIDXEntry);
				if (!entry) {
					e = GF_OUT_OF_MEM;
					goto exit;
				}
				entry->sidx = sidx;
				entry->start_offset = sidx_start;
				gf_list_add(daisy_sidx, entry);
			}
		}

		if (is_root_sidx) {
			root_sidx = sidx;
			sidx = NULL;
		}
		count = cur_idx = 0;
	}


	last_top_box_pos = root_prev_offset = sidx_end;
	sidx_idx = 0;
	sidx_dur = 0;
	local_sidx_start = local_sidx_end = 0;

	/*cumulated segments duration since start of the sidx */
	frag_count = frags_per_subsidx;
	cur_dur = 0;
	cur_idx = 0;
	first_frag_in_subseg = GF_TRUE;
	e = GF_OK;
	u64 cumulated_ref_size=0;
	while (gf_list_count(movie->moof_list)) {
		s32 offset_diff;
		u32 moof_size;

		movie->moof = (GF_MovieFragmentBox*)gf_list_get(movie->moof_list, 0);
		gf_list_rem(movie->moof_list, 0);
		movie->in_sidx_write = GF_TRUE;
		movie->moof->trun_ref_size=0;

		/*hierarchical or daisy-chain SIDXs*/
		if (!no_sidx && !sidx && (root_sidx || daisy_chain_sidx) ) {
			u32 subsegments_remaining;
			sidx = (GF_SegmentIndexBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_SIDX);
			if (!sidx) return GF_OUT_OF_MEM;
			sidx->reference_ID = referenceTrackID;
			sidx->timescale = trak ? trak->Media->mediaHeader->timeScale : 1000;
			sidx->earliest_presentation_time = get_presentation_time( ref_track_decode_time + sidx_dur + moof_get_earliest_cts(movie->moof, referenceTrackID), ts_shift);

			frag_count = frags_per_subsidx;

			/*last segment, add only one ref*/
			subsegments_remaining = 1 + gf_list_count(movie->moof_list);
			if (subseg_per_sidx*frags_per_subseg > subsegments_remaining) {
				subseg_per_sidx = subsegments_remaining / frags_per_subseg;
				if (subseg_per_sidx * frags_per_subseg < subsegments_remaining) subseg_per_sidx++;
			}
			/*we don't write anything between sidx and following moov*/
			sidx->first_offset = 0;
			sidx->nb_refs = subseg_per_sidx;
			if (daisy_chain_sidx && (nb_subsegs>1)) {
				sidx->nb_refs += 1;
			}
			sidx->refs = (GF_SIDXReference*)gf_malloc(sizeof(GF_SIDXReference)*sidx->nb_refs);
			if (!sidx->refs) return GF_OUT_OF_MEM;
			memset(sidx->refs, 0, sizeof(GF_SIDXReference)*sidx->nb_refs);

			if (root_sidx)
				root_sidx->refs[sidx_idx].reference_type = GF_TRUE;

			/*remember start of sidx*/
			local_sidx_start = gf_bs_get_position(movie->editFileMap->bs);

			/*write it*/
			e = gf_isom_box_size((GF_Box *) sidx);
			if (e) goto exit;
			e = gf_isom_box_write((GF_Box *) sidx, movie->editFileMap->bs);
			if (e) goto exit;

			local_sidx_end = gf_bs_get_position(movie->editFileMap->bs);

			/*adjust prev offset*/
			last_top_box_pos = local_sidx_end;

			if (daisy_sidx) {
				SIDXEntry *entry;
				GF_SAFEALLOC(entry, SIDXEntry);
				if (!entry) {
					e = GF_OUT_OF_MEM;
					goto exit;
				}
				entry->sidx = sidx;
				entry->start_offset = local_sidx_start;
				gf_list_add(daisy_sidx, entry);
			}
		}

		offset_diff = (s32) (gf_bs_get_position(movie->editFileMap->bs) - movie->moof->fragment_offset);
		movie->moof->fragment_offset = gf_bs_get_position(movie->editFileMap->bs);

		if (!e) {
			Bool generate_ssix = GF_FALSE;
			if (movie->root_ssix) generate_ssix = GF_TRUE;
			else if (use_ssix && ssix) generate_ssix = GF_TRUE;

			e = StoreFragment(movie, GF_FALSE, offset_diff, &moof_size, GF_FALSE);
			if (e) {
				e = GF_OUT_OF_MEM;
				goto exit;
			}

			if (sidx) {
				u32 cur_index = idx_offset + cur_idx;

				/*do not compute earliest CTS if single segment sidx since we only have set the info for one subsegment*/
				if (!movie->root_sidx && first_frag_in_subseg) {
					u64 first_cts = get_presentation_time( ref_track_decode_time + sidx_dur + cur_dur +  moof_get_earliest_cts(movie->moof, referenceTrackID), ts_shift);
					if (cur_index) {
						u32 subseg_dur = (u32) (first_cts - prev_earliest_cts);
						sidx->refs[cur_index-1].subsegment_duration = subseg_dur;
						if (root_sidx) root_sidx->refs[sidx_idx].subsegment_duration += subseg_dur;
					}
					prev_earliest_cts = first_cts;
					first_frag_in_subseg = GF_FALSE;
				}

				if (sidx->nb_refs<=cur_index) {
					sidx->nb_refs = cur_index+1;
					sidx->refs = gf_realloc(sidx->refs, sizeof(GF_SIDXReference)*sidx->nb_refs);
					memset(&sidx->refs[cur_index], 0, sizeof(GF_SIDXReference));
				}

				/*we refer to next moof*/
				sidx->refs[cur_index].reference_type = GF_FALSE;
				if (!sidx->refs[cur_index].SAP_type) {
					sidx->refs[cur_index].SAP_type = moof_get_sap_info(movie->moof, referenceTrackID, & sidx->refs[cur_index].SAP_delta_time, & sidx->refs[cur_index].starts_with_SAP);
					if (sidx->refs[cur_index].SAP_type) {
						if (root_sidx && !root_sidx->refs[sidx_idx].SAP_type) {
							root_sidx->refs[sidx_idx].SAP_type = sidx->refs[cur_index].SAP_type;
							root_sidx->refs[sidx_idx].SAP_delta_time = sidx->refs[cur_index].SAP_delta_time;
							root_sidx->refs[sidx_idx].starts_with_SAP = sidx->refs[cur_index].starts_with_SAP;
						}
					}
				}
				cur_dur += moof_get_duration(movie->moof, referenceTrackID);

				/*reference size is end of the moof we just wrote minus last_box_pos*/
				u64 last_pos = gf_bs_get_position(movie->editFileMap->bs);
				if (movie->moof->moof_data_len) {
					cumulated_ref_size += movie->moof->moof_data_len + movie->moof->trun_ref_size;
					last_pos += cumulated_ref_size;
				}
				sidx->refs[cur_index].reference_size += (u32) ( last_pos - last_top_box_pos) ;
				last_top_box_pos = last_pos;

				count++;

				if (generate_ssix) {
					u32 last_sseg_range0_size, remain_size;
					if (movie->root_ssix) {
						ssix = movie->root_ssix;
						if (ssix->subsegment_count <= cur_index) {
							assert(ssix->subsegment_count == cur_index);
							ssix->subsegment_count = cur_index+1;
							ssix->subsegment_alloc = ssix->subsegment_count;
							ssix->subsegments = gf_realloc(ssix->subsegments, ssix->subsegment_count * sizeof(GF_SubsegmentInfo));
							ssix->subsegments[cur_index].range_count = 2;
							ssix->subsegments[cur_index].ranges = gf_malloc(sizeof(GF_SubsegmentRangeInfo)*2);
						}
					}
					assert(ssix);
					ssix->subsegments[cur_index].ranges[0].level = 1;
					ssix->subsegments[cur_index].ranges[0].range_size = moof_get_first_sap_end(movie->moof);

					last_sseg_range0_size = (cur_index < ssix->subsegment_count) ? ssix->subsegments[cur_index].ranges[0].range_size : 0;
					ssix->subsegments[cur_index].ranges[1].level = 2;

					remain_size = sidx->refs[cur_index].reference_size - last_sseg_range0_size;
					ssix->subsegments[cur_index].ranges[1].range_size = remain_size;
					if (remain_size>0xFFFFFF) {
						GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso fragment] Remaining subsegment size %d larger than max ssix range size 0xFFFFFF, file might be broken\n", remain_size));
					}

					if (movie->root_ssix)
						ssix = NULL;
				}

				/*we are switching subsegment*/
				frag_count--;

				if (count==frags_per_subseg) {
					count = 0;
					first_frag_in_subseg = GF_TRUE;
					cur_idx++;
				}

				/*switching to next SIDX*/
				if ((cur_idx==subseg_per_sidx) || !frag_count) {
					u32 subseg_dur;
					/*update last ref duration*/

					//get next segment earliest cts - if estimation failed, use ref_track_next_cts
					if ((next_earliest_cts==-1) || (next_earliest_cts < prev_earliest_cts))  {
						u64 next_cts;
						if (gf_list_count(movie->moof_list)) {
							next_cts = get_presentation_time( ref_track_decode_time + sidx_dur + cur_dur + moof_get_earliest_cts((GF_MovieFragmentBox*)gf_list_get(movie->moof_list, 0), referenceTrackID), ts_shift);
						} else {
							next_cts = get_presentation_time( ref_track_next_cts, ts_shift);
						}
						subseg_dur = (u32) (next_cts - prev_earliest_cts);
					} else {
						subseg_dur = (u32) (next_earliest_cts - prev_earliest_cts);
					}

					if (movie->root_sidx) {
						sidx->refs[idx_offset].subsegment_duration = subseg_dur;
					}
					/*if daisy chain and not the last sidx, we have an extra entry at the end*/
					else if (daisy_chain_sidx && (nb_subsegs>1)) {
						sidx->refs[sidx->nb_refs - 2].subsegment_duration = subseg_dur;
					} else {
						sidx->refs[sidx->nb_refs-1].subsegment_duration = subseg_dur;
					}

					if (root_sidx) {

						root_sidx->refs[sidx_idx].subsegment_duration += subseg_dur;

						root_sidx->refs[sidx_idx].reference_size = (u32) (last_pos - local_sidx_start);
						if (!sidx_idx) {
							root_sidx->earliest_presentation_time = sidx->earliest_presentation_time;
						}
						sidx_rewrite(sidx, movie->editFileMap->bs, local_sidx_start, ssix);
						gf_isom_box_del((GF_Box*)sidx);
						sidx = NULL;
					} else if (daisy_chain_sidx) {
						SIDXEntry *entry = (SIDXEntry*)gf_list_last(daisy_sidx);
						entry->end_offset = last_pos;
						nb_subsegs--;
						sidx = NULL;
					}
					sidx_dur += cur_dur;
					cur_dur = 0;
					count = 0;
					cur_idx=0;
					if (movie->root_sidx)
						movie->root_sidx_index++;
					sidx_idx++;
				}
			}
		}
		if (movie->moof->moof_data_len) {
			if (!defer_moofs) defer_moofs = gf_list_new();
			gf_list_add(defer_moofs, movie->moof);
		} else {
			gf_isom_box_del((GF_Box *) movie->moof);
		}
		movie->moof = NULL;
	}
	movie->in_sidx_write = GF_FALSE;

	/*append segment marker box*/
	if (!defer_moofs && segment_marker_4cc) {
		gf_bs_write_u32(movie->editFileMap->bs, 8);	//write size field
		gf_bs_write_u32(movie->editFileMap->bs, segment_marker_4cc); //write box type field
	}

	if (movie->root_sidx) {
		if (last_segment && !movie->dyn_root_sidx) {
			assert(movie->root_sidx_index == movie->root_sidx->nb_refs);

			sidx_rewrite(movie->root_sidx, movie->editFileMap->bs, movie->root_sidx_offset, movie->root_ssix);
			gf_isom_box_del((GF_Box*) movie->root_sidx);
			movie->root_sidx = NULL;

			if (movie->root_ssix) {
				gf_isom_box_del((GF_Box*)movie->root_ssix);
				movie->root_ssix = NULL;
			}
		}
		if (ssix)
			gf_isom_box_del((GF_Box*)ssix);

		compute_seg_size(movie, out_seg_size);
		goto exit;
	}

	if (sidx) {
		assert(!root_sidx);
		sidx_rewrite(sidx, movie->editFileMap->bs, sidx_start, ssix);
		gf_isom_box_del((GF_Box*)sidx);
	}
	if (ssix) {
		gf_isom_box_del((GF_Box*)ssix);
		ssix = NULL;
	}

	if (daisy_sidx) {
		u32 i, j;
		u64 last_entry_end_offset = 0;

		count = gf_list_count(daisy_sidx);
		for (i=count; i>1; i--) {
			SIDXEntry *entry = (SIDXEntry*)gf_list_get(daisy_sidx, i-2);
			SIDXEntry *next_entry = (SIDXEntry*)gf_list_get(daisy_sidx, i-1);

			if (!last_entry_end_offset) {
				last_entry_end_offset = next_entry->end_offset;
				/*rewrite last sidx*/
				sidx_rewrite(next_entry->sidx, movie->editFileMap->bs, next_entry->start_offset, NULL);
			}
			/*copy over SAP info for last item (which points to next item !)*/
			entry->sidx->refs[entry->sidx->nb_refs-1] = next_entry->sidx->refs[0];
			/*and rewrite reference type, size and dur*/
			entry->sidx->refs[entry->sidx->nb_refs-1].reference_type = GF_TRUE;
			entry->sidx->refs[entry->sidx->nb_refs-1].reference_size = (u32) (last_entry_end_offset - next_entry->start_offset);
			entry->sidx->refs[entry->sidx->nb_refs-1].subsegment_duration = 0;
			for (j=0; j<next_entry->sidx->nb_refs; j++) {
				entry->sidx->refs[entry->sidx->nb_refs-1].subsegment_duration += next_entry->sidx->refs[j].subsegment_duration;
			}
			sidx_rewrite(entry->sidx, movie->editFileMap->bs, entry->start_offset, NULL);
		}
		while (gf_list_count(daisy_sidx)) {
			SIDXEntry *entry = (SIDXEntry*)gf_list_last(daisy_sidx);
			gf_isom_box_del((GF_Box*)entry->sidx);
			gf_free(entry);
			gf_list_rem_last(daisy_sidx);
		}
		gf_list_del(daisy_sidx);
	}
	if (root_sidx) {
		sidx_rewrite(root_sidx, movie->editFileMap->bs, sidx_start, NULL);
		gf_isom_box_del((GF_Box*)root_sidx);
	}

	if ((root_sidx || sidx) && !daisy_chain_sidx) {
		if (index_start_range) *index_start_range = sidx_start;
		if (index_end_range) *index_end_range = sidx_end - 1;
	}

	if (movie->append_segment) {
		char bloc[1024];
		u32 seg_size = (u32) gf_bs_get_size(movie->editFileMap->bs);
		gf_bs_seek(movie->editFileMap->bs, 0);
		while (seg_size) {
			u32 size = gf_bs_read_data(movie->editFileMap->bs, bloc, (seg_size>1024) ? 1024 : seg_size);
			gf_bs_write_data(movie->movieFileMap->bs, bloc, size);
			seg_size -= size;
		}
		gf_isom_datamap_del(movie->editFileMap);
		movie->editFileMap = gf_isom_fdm_new_temp(NULL);
	} else if (close_segment_handle == GF_TRUE) {
		gf_isom_datamap_del(movie->editFileMap);
		movie->editFileMap = NULL;
	}
	compute_seg_size(movie, out_seg_size);

exit:
	if (movie->editFileMap && (orig_bs != movie->editFileMap->bs)) {
		u32 tmpsize;
		gf_bs_get_content_no_truncate(movie->editFileMap->bs, &movie->block_buffer, &tmpsize, &movie->block_buffer_size);
		gf_bs_del(movie->editFileMap->bs);
		movie->editFileMap->bs = orig_bs;
	}
	//flush all defered
	if (defer_moofs) {
		while (gf_list_count(defer_moofs)) {
			movie->moof = gf_list_pop_front(defer_moofs);
			movie->on_block_out(movie->on_block_out_usr_data, movie->moof->moof_data, movie->moof->moof_data_len, NULL, 0);
			if (out_seg_size) *out_seg_size += movie->moof->moof_data_len;

			flush_ref_samples(movie, out_seg_size, segment_marker_4cc ? GF_TRUE : GF_FALSE);

			gf_free(movie->moof->moof_data);
			gf_isom_box_del((GF_Box *) movie->moof);
			movie->moof = NULL;
		}
		gf_list_del(defer_moofs);

		if (segment_marker_4cc) {
			char seg[8];
			if (movie->on_last_block_start)
				movie->on_last_block_start(movie->on_block_out_usr_data);

			seg[0] = seg[1] = seg[2] = 0;
			seg[3] = 9;
			seg[4] = (segment_marker_4cc>>24) & 0xFF;
			seg[5] = (segment_marker_4cc>>16) & 0xFF;
			seg[6] = (segment_marker_4cc>>8) & 0xFF;
			seg[7] = (segment_marker_4cc) & 0xFF;
			movie->on_block_out(movie->on_block_out_usr_data, seg, 8, NULL, 0);
			if (out_seg_size)
				*out_seg_size += 8;
		}
		gf_bs_seek(movie->editFileMap->bs, 0);
	}
	return e;
}

GF_EXPORT
GF_Err gf_isom_flush_sidx(GF_ISOFile *movie, u32 sidx_max_size, Bool force_v1)
{
	GF_BitStream *bs;
	GF_Err e;
	u32 size;
	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	if (! movie->on_block_out) return GF_BAD_PARAM;
	if (! movie->root_sidx) return GF_BAD_PARAM;

	if (!movie->block_buffer_size) movie->block_buffer_size = movie->on_block_out_block_size;
	bs = gf_bs_new_cbk_buffer(isom_on_block_out, movie, movie->block_buffer, movie->block_buffer_size);
	gf_bs_prevent_dispatch(bs, GF_TRUE);
	
	assert(movie->root_sidx_index == movie->root_sidx->nb_refs);

	if (force_v1)
		movie->root_sidx->version = 1;
		
	e = gf_isom_box_size((GF_Box*)movie->root_sidx);
	size = (u32) movie->root_sidx->size;
	if (movie->root_ssix) {
		e = gf_isom_box_size((GF_Box*)movie->root_ssix);
		size += (u32) movie->root_ssix->size;
		movie->root_sidx->first_offset = (u32) movie->root_ssix->size;
	}

	if (sidx_max_size && (size > sidx_max_size) ) {
#ifndef GPAC_DISABLE_LOG
		u32 orig_seg_count = movie->root_sidx->nb_refs;
#endif
		//trash 8 bytes to be able to write a free box before
		sidx_max_size -= 8;
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso fragment] SIDX size %d is larger than allocated SIDX block %d, merging final segments\n", movie->root_sidx->size, sidx_max_size));
		while (movie->root_sidx->nb_refs>2) {
			movie->root_sidx->refs[movie->root_sidx->nb_refs-2].subsegment_duration += movie->root_sidx->refs[movie->root_sidx->nb_refs-1].subsegment_duration;
			movie->root_sidx->refs[movie->root_sidx->nb_refs-2].reference_size += movie->root_sidx->refs[movie->root_sidx->nb_refs-1].reference_size;
			movie->root_sidx->nb_refs--;
			if (movie->root_ssix) {
				movie->root_ssix->subsegments[movie->root_ssix->subsegment_count-2].ranges[1].range_size += movie->root_ssix->subsegments[movie->root_ssix->subsegment_count-1].ranges[0].range_size;
				movie->root_ssix->subsegments[movie->root_ssix->subsegment_count-2].ranges[1].range_size += movie->root_ssix->subsegments[movie->root_ssix->subsegment_count-1].ranges[1].range_size;
				movie->root_ssix->subsegment_count--;
			}

			e = gf_isom_box_size((GF_Box*)movie->root_sidx);
			size = (u32) movie->root_sidx->size;
			if (movie->root_ssix) {
				e = gf_isom_box_size((GF_Box*)movie->root_ssix);
				size += (u32) movie->root_ssix->size;
				movie->root_sidx->first_offset = (u32) movie->root_ssix->size;
			}

			if (size < sidx_max_size) break;
		}
		if (size > sidx_max_size) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso fragment] SIDX size %d is larger than allocated SIDX block and no more segments to merge\n", size, sidx_max_size));
			return GF_IO_ERR;
		} else {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso fragment] Merged %d segments in SIDX to fit allocated block, remaining segments %d\n", orig_seg_count - movie->root_sidx->nb_refs, movie->root_sidx->nb_refs));
		}
	}
	if (!e) {
		if (movie->root_ssix) {
			gf_isom_box_size((GF_Box *) movie->root_ssix);

			if (movie->compress_mode>=GF_ISOM_COMP_MOOF_SSIX) {
				u32 ssix_comp_size;
				//compute ssix compressed size by using NULL destination bitstream
				//not really optimum since we compress twice the ssix, to optimize ...
				e = gf_isom_write_compressed_box(movie, (GF_Box *) movie->root_ssix, GF_4CC('!', 's', 's', 'x'), NULL, &ssix_comp_size);
				movie->root_sidx->first_offset = ssix_comp_size;
			} else {
				movie->root_sidx->first_offset = (u32) movie->root_ssix->size;
			}
		}
		if (!e) {
			if (movie->compress_mode>=GF_ISOM_COMP_MOOF_SIDX) {
				e = gf_isom_write_compressed_box(movie, (GF_Box *) movie->root_sidx, GF_4CC('!', 's', 'i', 'x'), bs, NULL);
			} else {
				e = gf_isom_box_write((GF_Box *) movie->root_sidx, bs);
			}
		}

		if (!e && movie->root_ssix) {
			if (movie->compress_mode>=GF_ISOM_COMP_MOOF_SSIX) {
				e = gf_isom_write_compressed_box(movie, (GF_Box *) movie->root_ssix, GF_4CC('!', 's', 's', 'x'), bs, NULL);
			} else {
				e = gf_isom_box_write((GF_Box *) movie->root_ssix, bs);
			}
		}
	}

	gf_isom_box_del((GF_Box*) movie->root_sidx);
	movie->root_sidx = NULL;
	if (movie->root_ssix) {
		gf_isom_box_del((GF_Box*) movie->root_ssix);
		movie->root_ssix = NULL;
	}

	gf_bs_get_content_no_truncate(bs, &movie->block_buffer, &size, &movie->block_buffer_size);
	gf_bs_del(bs);
	return e;
}

GF_EXPORT
GF_Err gf_isom_close_fragments(GF_ISOFile *movie)
{
	if (movie->use_segments) {
		return gf_isom_close_segment(movie, 0, 0, 0, 0, 0, 0, GF_FALSE, GF_FALSE, 1, 0, NULL, NULL, NULL);
	} else {
		return StoreFragment(movie, GF_FALSE, 0, NULL, GF_TRUE);
	}
}

GF_EXPORT
GF_Err gf_isom_start_segment(GF_ISOFile *movie, const char *SegName, Bool memory_mode)
{
	GF_Err e;
	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	if (gf_list_count(movie->moof_list))
		return GF_BAD_PARAM;

	movie->segment_bs = NULL;
	movie->append_segment = GF_FALSE;
	/*update segment file*/
	if (SegName || !gf_isom_get_filename(movie)) {
		if (movie->editFileMap) gf_isom_datamap_del(movie->editFileMap);
		e = gf_isom_datamap_new(SegName, NULL, GF_ISOM_DATA_MAP_WRITE, &movie->editFileMap);
		movie->segment_start = 0;
		movie->write_styp = 1;
		if (e) return e;
	} else {
		assert(gf_list_count(movie->moof_list) == 0);
		movie->segment_start = gf_bs_get_position(movie->editFileMap->bs);
		/*if movieFileMap is not null, we are concatenating segments to the original movie, force a copy*/
		if (movie->movieFileMap)
			movie->append_segment = GF_TRUE;
		movie->write_styp = 0;
	}

	/*create a memory bitstream for all file IO until final flush*/
	if (memory_mode) {
		movie->segment_bs = movie->editFileMap->bs;
		movie->editFileMap->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_fragment_reference_time(GF_ISOFile *movie, GF_ISOTrackID reference_track_ID, u64 ntp, u64 timestamp)
{
	if (!movie || !movie->moof) return GF_BAD_PARAM;
	movie->moof->reference_track_ID = reference_track_ID;
	movie->moof->ntp = ntp;
	movie->moof->timestamp = timestamp;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_traf_mss_timeext(GF_ISOFile *movie, GF_ISOTrackID reference_track_ID, u64 ntp_in_track_timescale, u64 traf_duration_in_track_timescale)
{
	u32 i;
	if (!movie || !movie->moof)
		return GF_BAD_PARAM;
	for (i=0; i<gf_list_count(movie->moof->TrackList); i++) {
		GF_TrackFragmentBox *traf = (GF_TrackFragmentBox*)gf_list_get(movie->moof->TrackList, i);
		if (!traf)
			return GF_BAD_PARAM;
		if (traf->tfxd)
			gf_isom_box_del_parent(&traf->child_boxes, (GF_Box*)traf->tfxd);
		traf->tfxd = (GF_MSSTimeExtBox *)gf_isom_box_new_parent(&traf->child_boxes, GF_ISOM_BOX_UUID_TFXD);
		if (!traf->tfxd) return GF_OUT_OF_MEM;
		traf->tfxd->absolute_time_in_track_timescale = ntp_in_track_timescale;
		traf->tfxd->fragment_duration_in_track_timescale = traf_duration_in_track_timescale;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_start_fragment(GF_ISOFile *movie, GF_ISOStartFragmentFlags flags)
{
	u32 i, count;
	GF_TrackExtendsBox *trex;
	GF_TrackFragmentBox *traf;
	GF_Err e;
	Bool moof_first = (flags & GF_ISOM_FRAG_MOOF_FIRST) ? GF_TRUE : GF_FALSE;
#ifdef GF_ENABLE_CTRN
	Bool use_ctrn = (flags & GF_ISOM_FRAG_USE_COMPACT) ? GF_TRUE : GF_FALSE;
#endif

	//and only at setup
	if (!movie || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) )
		return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_WRITE) return GF_ISOM_INVALID_MODE;

	count = gf_list_count(movie->moov->mvex->TrackExList);
	if (!count)
		return GF_BAD_PARAM;

	/*always force cached mode when writing movie segments*/
	if (movie->use_segments) moof_first = GF_TRUE;
	movie->moof_first = moof_first;

	//store existing fragment
	if (movie->moof) {
		e = StoreFragment(movie, movie->use_segments ? GF_TRUE : GF_FALSE, 0, NULL, movie->use_segments ? GF_TRUE : (movie->on_block_out ? GF_TRUE : GF_FALSE));
		if (e) return e;
	}

	//create new fragment
	movie->moof = (GF_MovieFragmentBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MOOF);
	if (!movie->moof) return GF_OUT_OF_MEM;
	movie->moof->mfhd = (GF_MovieFragmentHeaderBox *) gf_isom_box_new_parent(&movie->moof->child_boxes, GF_ISOM_BOX_TYPE_MFHD);
	if (!movie->moof->mfhd) return GF_OUT_OF_MEM;
	movie->moof->mfhd->sequence_number = movie->NextMoofNumber;
	movie->NextMoofNumber ++;
	if (movie->use_segments || movie->on_block_out)
		gf_list_add(movie->moof_list, movie->moof);


	/*remember segment offset*/
	movie->moof->fragment_offset = gf_bs_get_position(movie->editFileMap->bs);

	/*prepare MDAT*/
	gf_bs_write_u32(movie->editFileMap->bs, 0);
	gf_bs_write_u32(movie->editFileMap->bs, GF_ISOM_BOX_TYPE_MDAT);

	//we create a TRAF for each setup track, unused ones will be removed at store time
	for (i=0; i<count; i++) {
		trex = (GF_TrackExtendsBox*)gf_list_get(movie->moov->mvex->TrackExList, i);
		traf = (GF_TrackFragmentBox *) gf_isom_box_new_parent(&movie->moof->child_boxes, GF_ISOM_BOX_TYPE_TRAF);
		if (!traf) return GF_OUT_OF_MEM;
		traf->trex = trex;
		traf->tfhd = (GF_TrackFragmentHeaderBox *) gf_isom_box_new_parent(&traf->child_boxes, GF_ISOM_BOX_TYPE_TFHD);
		if (!traf->tfhd) return GF_OUT_OF_MEM;
		traf->tfhd->trackID = trex->trackID;
		//add 8 bytes (MDAT size+type) to avoid the data_offset in the first trun
		traf->tfhd->base_data_offset = movie->moof->fragment_offset + 8;
#ifdef GF_ENABLE_CTRN
		traf->use_ctrn = use_ctrn;
		if (trex->inherit_from_traf_id)
			traf->use_inherit = GF_TRUE;
#endif
		gf_list_add(movie->moof->TrackList, traf);

		if (movie->mfra) {
			GF_TrackFragmentRandomAccessBox *tfra;
			GF_RandomAccessEntry *raf;
			if (!traf->trex->tfra) {
				tfra = (GF_TrackFragmentRandomAccessBox *)gf_isom_box_new_parent(&movie->mfra->child_boxes, GF_ISOM_BOX_TYPE_TFRA);
				if (!tfra) return GF_OUT_OF_MEM;
				tfra->track_id = traf->trex->trackID;
				tfra->traf_bits = 8;
				tfra->trun_bits = 8;
				tfra->sample_bits = 8;
				gf_list_add(movie->mfra->tfra_list, tfra);
				traf->trex->tfra = tfra;
			}
			tfra = traf->trex->tfra;
			tfra->entries = (GF_RandomAccessEntry *)gf_realloc(tfra->entries, (tfra->nb_entries+1)*sizeof(GF_RandomAccessEntry));
			tfra->nb_entries++;
			raf = &tfra->entries[tfra->nb_entries-1];
			raf->sample_number = 1;
			raf->time = 0;
			raf->traf_number = i+1;
			//trun number is set once we fond a sync
			raf->trun_number = 0;
			if (!strcmp(movie->fileName, "_gpac_isobmff_redirect")) {
				raf->moof_offset = movie->fragmented_file_pos;
			} else {
				raf->moof_offset = movie->moof->fragment_offset;
			}
		}
	}
	return GF_OK;
}


GF_Err gf_isom_set_fragment_template(GF_ISOFile *movie, u8 *tpl_data, u32 tpl_size, Bool *has_tfdt, GF_SegmentIndexBox **out_sidx)
{
	GF_BitStream *bs;
	GF_Err e=GF_OK;
	if (out_sidx) *out_sidx = NULL;
	if (!movie->moof) return GF_BAD_PARAM;

	bs = gf_bs_new(tpl_data, tpl_size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		GF_Box *a;
		e = gf_isom_box_parse(&a, bs);
		if (e) break;
		if (a->type==GF_ISOM_BOX_TYPE_STYP) {
			if (movie->brand) {
				gf_list_del_item(movie->TopBoxes, movie->brand);
				gf_isom_box_del((GF_Box *)movie->brand);
			}
			movie->brand = (GF_FileTypeBox *) a;
			gf_list_add(movie->TopBoxes, movie->brand);
			movie->write_styp = 2;
			continue;
		}
		if (a->type==GF_ISOM_BOX_TYPE_OTYP) {
			if (movie->otyp) {
				gf_list_del_item(movie->TopBoxes, movie->otyp);
				gf_isom_box_del(movie->otyp);
			}
			movie->otyp = (GF_Box *) a;
			gf_list_add(movie->TopBoxes, movie->otyp);
			continue;
		}
		if (a->type==GF_ISOM_BOX_TYPE_MOOF) {
			u32 i, count, j, nb_trex;
			s32 idx;
			Bool single_track=GF_FALSE;
			GF_MovieFragmentBox *moof = (GF_MovieFragmentBox *)a;

			moof->fragment_offset = movie->moof->fragment_offset;
			nb_trex = gf_list_count(movie->moov->mvex->TrackExList);
			count = gf_list_count(moof->TrackList);
			for (i=0; i<count; i++) {
				GF_TrackFragmentBox *traf = gf_list_get(moof->TrackList, i);
				GF_TrackBox *trak = traf->tfhd ? gf_isom_get_track_from_id(movie->moov, traf->tfhd->trackID) : NULL;
				if (traf->tfhd && !trak && !single_track && (gf_list_count(movie->moov->trackList)==1)) {
					trak = gf_list_get(movie->moov->trackList, 0);
					single_track = GF_TRUE;
					traf->tfhd->trackID = trak->Header->trackID;
				}
				for (j=0; j<nb_trex && trak; j++) {
					GF_TrackExtendsBox *trex = gf_list_get(movie->moov->mvex->TrackExList, j);
					if (trex->trackID == traf->tfhd->trackID) {
						traf->trex = trex;
						break;
					}
				}
				if (!trak || !traf->trex) {
					gf_list_rem(moof->TrackList, i);
					i--;
					count--;
					gf_isom_box_del((GF_Box*)traf);
					continue;
				}
				traf->tfhd->base_data_offset = movie->moof->fragment_offset + 8;
				if (traf->tfdt && has_tfdt)
					*has_tfdt = GF_TRUE;
			}
			//remove old moof and switch with this one
			idx = gf_list_find(movie->moof_list, movie->moof);
			if (idx>=0) {
				gf_list_rem(movie->moof_list, idx);
				gf_list_add(movie->moof_list, moof);
			}
			gf_isom_box_del((GF_Box *)movie->moof);
			movie->moof = moof;
			continue;
		}
		if (a->type==GF_ISOM_BOX_TYPE_SIDX) {
			if (out_sidx && !*out_sidx) {
				*out_sidx = (GF_SegmentIndexBox *) a;
				continue;
			}
		}
		gf_isom_box_del(a);
	}
	gf_bs_del(bs);
	return e;
}

GF_EXPORT
GF_Err gf_isom_fragment_add_sample_ex(GF_ISOFile *movie, GF_ISOTrackID TrackID, const GF_ISOSample *sample, u32 DescIndex,
                                   u32 Duration, u8 PaddingBits, u16 DegradationPriority, Bool redundant_coding, void **ref, u32 ref_offset)
{
	u32 count, buffer_size;
	u8 *buffer;
	u64 pos;
	GF_ISOSample *od_sample = NULL;
	GF_TrunEntry ent, *prev_ent;
	GF_TrackFragmentBox *traf, *traf_2;
	GF_TrackFragmentRunBox *trun;

	if (!movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) || !sample)
		return GF_BAD_PARAM;

	traf = gf_isom_get_traf(movie, TrackID);
	if (!traf)
		return GF_BAD_PARAM;

	if (!traf->tfhd->sample_desc_index)
		traf->tfhd->sample_desc_index = DescIndex ? DescIndex : traf->trex->def_sample_desc_index;

	pos = gf_bs_get_position(movie->editFileMap->bs);


	//WARNING: we change stream description, create a new TRAF
	if ( DescIndex && (traf->tfhd->sample_desc_index != DescIndex)) {
		//if we're caching flush the current run
		if (traf->DataCache && !traf->use_sample_interleave) {
			count = gf_list_count(traf->TrackRuns);
			if (count) {
				trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
				trun->data_offset = (u32) (pos - movie->moof->fragment_offset - 8);
				gf_bs_get_content(trun->cache, &buffer, &buffer_size);
				gf_bs_write_data(movie->editFileMap->bs, buffer, buffer_size);
				gf_bs_del(trun->cache);
				trun->cache = NULL;
				gf_free(buffer);
			}
		}
		traf_2 = (GF_TrackFragmentBox *) gf_isom_box_new_parent(&movie->moof->child_boxes, GF_ISOM_BOX_TYPE_TRAF);
		if (!traf_2) return GF_OUT_OF_MEM;
		traf_2->trex = traf->trex;
		traf_2->tfhd = (GF_TrackFragmentHeaderBox *) gf_isom_box_new_parent(&traf_2->child_boxes, GF_ISOM_BOX_TYPE_TFHD);
		if (!traf_2->tfhd) return GF_OUT_OF_MEM;
		traf_2->tfhd->trackID = traf->tfhd->trackID;
		//keep the same offset
		traf_2->tfhd->base_data_offset = movie->moof->fragment_offset + 8;
		gf_list_add(movie->moof->TrackList, traf_2);

		//duplicate infos
		traf_2->IFrameSwitching = traf->IFrameSwitching;
		traf_2->use_sample_interleave = traf->use_sample_interleave;
		traf_2->interleave_id = traf->interleave_id;
		traf_2->truns_first = traf->truns_first;
		traf_2->truns_v1 = traf->truns_v1;
		traf_2->large_tfdt = traf->large_tfdt;
		traf_2->DataCache  = traf->DataCache;
		traf_2->tfhd->sample_desc_index  = DescIndex;

		//switch them ...
		traf = traf_2;
	}

	pos = movie->moof->trun_ref_size ? (8+movie->moof->trun_ref_size) : gf_bs_get_position(movie->editFileMap->bs);

	//check if we need a new trun entry
	count = (traf->use_sample_interleave && traf->force_new_trun) ? 0 : gf_list_count(traf->TrackRuns);
	if (count) {
		trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
		//check data offset when no caching as trun entries shall ALWAYS be contiguous samples
		if (!traf->DataCache && (movie->moof->fragment_offset + 8 + trun->data_offset + trun->run_size != pos) )
			count = 0;

		//check I-frame detection
		if (traf->IFrameSwitching && sample->IsRAP)
			count = 0;

		if (traf->DataCache && (traf->DataCache==trun->sample_count) && !traf->use_sample_interleave)
			count = 0;

		if (traf->force_new_trun)
			count = 0;

		//if data cache is on and we're changing TRUN, store the cache and update data offset
		if (!count && traf->DataCache && !traf->use_sample_interleave) {
			trun->data_offset = (u32) (pos - movie->moof->fragment_offset - 8);
			gf_bs_get_content(trun->cache, &buffer, &buffer_size);
			gf_bs_write_data(movie->editFileMap->bs, buffer, buffer_size);
			gf_bs_del(trun->cache);
			trun->cache = NULL;
			gf_free(buffer);
		}
	}
	traf->force_new_trun = 0;

	//new run
	if (!count) {
		trun = (GF_TrackFragmentRunBox *) gf_isom_box_new_parent(&traf->child_boxes, GF_ISOM_BOX_TYPE_TRUN);
		if (!trun) return GF_OUT_OF_MEM;
		//store data offset (we have the 8 btyes offset of the MDAT)
		trun->data_offset = (u32) (pos - movie->moof->fragment_offset - 8);
		gf_list_add(traf->TrackRuns, trun);
#ifdef GF_ENABLE_CTRN
		trun->use_ctrn = traf->use_ctrn;
		trun->use_inherit = traf->use_inherit;
		trun->ctso_multiplier = traf->trex->def_sample_duration;
#endif
		trun->interleave_id = traf->interleave_id;
		if (traf->truns_v1)
			trun->version = 1;

		//if we use data caching, create a bitstream
		if (traf->DataCache)
			trun->cache = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	}

	memset(&ent, 0, sizeof(GF_TrunEntry));
	ent.CTS_Offset = sample->CTS_Offset;
	ent.Duration = Duration;
	ent.dts = sample->DTS;
	ent.nb_pack = sample->nb_pack;
	ent.flags = GF_ISOM_FORMAT_FRAG_FLAGS(PaddingBits, sample->IsRAP, DegradationPriority);
	if (sample->IsRAP) {
		ent.flags |= GF_ISOM_GET_FRAG_DEPEND_FLAGS(0, 2, 0, (redundant_coding ? 1 : 0) );
		ent.SAP_type = sample->IsRAP;
	}
	if (trun->nb_samples) {
		prev_ent = &trun->samples[trun->nb_samples-1];
	} else {
		prev_ent = NULL;
	}

	if (prev_ent && (prev_ent->dts || !prev_ent->Duration) && sample->DTS) {
		u32 nsamp = prev_ent->nb_pack ? prev_ent->nb_pack : 1;
		if (nsamp*prev_ent->Duration != sample->DTS - prev_ent->dts)
			prev_ent->Duration = (u32) (sample->DTS - prev_ent->dts) / nsamp;
	}
	if (trun->nb_samples >= trun->sample_alloc) {
		trun->sample_alloc += 50;
		if (trun->nb_samples >= trun->sample_alloc) trun->sample_alloc = trun->nb_samples+1;
		trun->samples = gf_realloc(trun->samples, sizeof(GF_TrunEntry)*trun->sample_alloc);
		if (!trun->samples) return GF_OUT_OF_MEM;
	}

	//rewrite OD frames
	if (traf->trex->track->Media->handler->handlerType == GF_ISOM_MEDIA_OD) {
		//this may fail if dependencies are not well done ...
		Media_ParseODFrame(traf->trex->track->Media, sample, &od_sample);
		sample = od_sample;
	}

	ent.size = sample->dataLength;
	trun->samples[trun->nb_samples] = ent;
	trun->nb_samples ++;
	trun->run_size += ent.size;

	if (sample->CTS_Offset<0) {
		trun->version = 1;
	}
	trun->sample_count += sample->nb_pack ? sample->nb_pack : 1;

	//finally write the data
	if (sample->dataLength) {
		u32 res = 0;
		if (!traf->DataCache) {
			if (movie->moof_first && movie->on_block_out && (ref || trun->sample_refs)) {
				GF_TrafSampleRef *sref;
				if (!trun->sample_refs) trun->sample_refs = gf_list_new();
				GF_SAFEALLOC(sref, GF_TrafSampleRef);
				if (!sref) return GF_OUT_OF_MEM;
				if (ref && *ref && !od_sample) {
					sref->data = sample->data;
					sref->len = sample->dataLength;
					sref->ref = *ref;
					sref->ref_offset = ref_offset;
					*ref = NULL;
				} else {
					sref->data = gf_malloc(sample->dataLength);
					if (!sref->data) {
						gf_free(sref);
						return GF_OUT_OF_MEM;
					}
					memcpy(sref->data, sample->data, sample->dataLength);
					sref->len = sample->dataLength;
				}
				res = sref->len;
				traf->trun_ref_size += res;
				movie->moof->trun_ref_size += res;
				gf_list_add(trun->sample_refs, sref);
			} else {
				res = gf_bs_write_data(movie->editFileMap->bs, sample->data, sample->dataLength);
			}
		} else if (trun->cache) {
			res = gf_bs_write_data(trun->cache, sample->data, sample->dataLength);
		} else {
			return GF_BAD_PARAM;
		}
		if (res!=sample->dataLength) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso fragment] Could not add a sample with a size of %u bytes\n", sample->dataLength));
			return GF_OUT_OF_MEM;
		}

	}
	if (od_sample) gf_isom_sample_del(&od_sample);

	if (traf->trex->tfra) {
		GF_RandomAccessEntry *raf;
		raf = &traf->trex->tfra->entries[traf->trex->tfra->nb_entries-1];
		//if sample is sync, store its time and trun number
		if (!raf->trun_number && sample->IsRAP) {
			raf->time = sample->DTS + sample->CTS_Offset;
			raf->trun_number = gf_list_count(traf->TrackRuns);
			raf->sample_number = trun->sample_count;
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_fragment_add_sample(GF_ISOFile *movie, GF_ISOTrackID TrackID, const GF_ISOSample *sample, u32 DescIndex,
                                   u32 Duration, u8 PaddingBits, u16 DegradationPriority, Bool redundant_coding)
{
	return gf_isom_fragment_add_sample_ex(movie, TrackID, sample, DescIndex, Duration, PaddingBits, DegradationPriority, redundant_coding, NULL, 0);

}
GF_EXPORT
GF_Err gf_isom_fragment_set_cenc_sai(GF_ISOFile *output, GF_ISOTrackID TrackID, u8 *sai_b, u32 sai_b_size, Bool use_subsamples, Bool use_saio_32bit, Bool use_multikey)
{
	GF_CENCSampleAuxInfo *sai;
	GF_TrackFragmentBox  *traf = gf_isom_get_traf(output, TrackID);
	GF_SampleEncryptionBox *senc;

	if (!traf)  return GF_BAD_PARAM;

	if (!traf->sample_encryption) {
		if (!traf->trex->track->sample_encryption) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isofile] trying to add CENC SAI without storage box allocated\n" ));
			return GF_BAD_PARAM;
		}
		if (traf->trex->track->sample_encryption->type == GF_ISOM_BOX_TYPE_SENC) {
			traf->sample_encryption = gf_isom_create_samp_enc_box(0, 0);
		} else {
			GF_SampleEncryptionBox *psec = (GF_SampleEncryptionBox *) traf->trex->track->sample_encryption;
			if (!psec) return GF_ISOM_INVALID_FILE;
			traf->sample_encryption = gf_isom_create_piff_psec_box(1, 0, psec->AlgorithmID, psec->IV_size, psec->KID);
		}
		if (!traf->sample_encryption) return GF_OUT_OF_MEM;
		traf->sample_encryption->traf = traf;

		if (!traf->child_boxes) traf->child_boxes = gf_list_new();
		gf_list_add(traf->child_boxes, traf->sample_encryption);
	}
	senc = (GF_SampleEncryptionBox *) traf->sample_encryption;

	if (!sai_b_size && !sai_b) {
		gf_isom_cenc_set_saiz_saio(senc, NULL, traf, 0, use_saio_32bit, use_multikey);
		return GF_OK;
	}

	GF_SAFEALLOC(sai, GF_CENCSampleAuxInfo);
	if (!sai) return GF_OUT_OF_MEM;

	if (sai_b && sai_b_size) {
		sai->cenc_data_size = sai_b_size;
		sai->cenc_data = gf_malloc(sizeof(u8) * sai_b_size);
		if (!sai->cenc_data) {
			gf_free(sai);
			return GF_OUT_OF_MEM;
		}
		memcpy(sai->cenc_data, sai_b, sai_b_size);
	} else {
		sai->isNotProtected = 1;
	}

	gf_list_add(senc->samp_aux_info, sai);
	if (use_subsamples)
		senc->flags = 0x00000002;
	if (use_multikey)
		senc->version = 1;

	gf_isom_cenc_set_saiz_saio(senc, NULL, traf, sai->cenc_data_size, use_saio_32bit, use_multikey);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_fragment_append_data_ex(GF_ISOFile *movie, GF_ISOTrackID TrackID, u8 *data, u32 data_size, u8 PaddingBits, void **ref, u32 ref_offset)
{
	u32 count;
	u8 rap;
	u16 degp;
	GF_TrunEntry *ent;
	GF_TrackFragmentBox *traf;
	GF_TrackFragmentRunBox *trun;
	if (!movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	traf = gf_isom_get_traf(movie, TrackID);
	if (!traf || !traf->tfhd->sample_desc_index) return GF_BAD_PARAM;

	//add TRUN entry
	count = gf_list_count(traf->TrackRuns);
	if (!count) return GF_BAD_PARAM;

	trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
	if (!trun->nb_samples) return GF_BAD_PARAM;
	ent = &trun->samples[trun->nb_samples-1];
	ent->size += data_size;
	trun->run_size += data_size;

	rap = GF_ISOM_GET_FRAG_SYNC(ent->flags);
	degp = GF_ISOM_GET_FRAG_DEG(ent->flags);
	ent->flags = GF_ISOM_FORMAT_FRAG_FLAGS(PaddingBits, rap, degp);

	//finally write the data
	if (!traf->DataCache) {
		if (movie->moof_first && movie->on_block_out && (ref || trun->sample_refs)) {
			GF_TrafSampleRef *sref;
			if (!trun->sample_refs) trun->sample_refs = gf_list_new();
			GF_SAFEALLOC(sref, GF_TrafSampleRef);
			if (!sref) return GF_OUT_OF_MEM;
			if (ref && *ref) {
				sref->data = data;
				sref->len = data_size;
				sref->ref = *ref;
				sref->ref_offset = ref_offset;
				*ref = NULL;
			} else {
				sref->data = gf_malloc(data_size);
				if (!sref->data) {
					gf_free(sref);
					return GF_OUT_OF_MEM;
				}
				memcpy(sref->data, data, data_size);
				sref->len = data_size;
			}
			traf->trun_ref_size += sref->len;
			movie->moof->trun_ref_size += sref->len;
			gf_list_add(trun->sample_refs, sref);
		} else {
			gf_bs_write_data(movie->editFileMap->bs, data, data_size);
		}
	} else if (trun->cache) {
		gf_bs_write_data(trun->cache, data, data_size);
	} else {
		return GF_BAD_PARAM;
	}
	return GF_OK;
}
GF_EXPORT
GF_Err gf_isom_fragment_append_data(GF_ISOFile *movie, GF_ISOTrackID TrackID, u8 *data, u32 data_size, u8 PaddingBits)
{
	return gf_isom_fragment_append_data_ex(movie, TrackID, data, data_size, PaddingBits, NULL, 0);

}

GF_Err gf_isom_fragment_add_subsample(GF_ISOFile *movie, GF_ISOTrackID TrackID, u32 flags, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable)
{
	u32 i, count, last_sample;
	GF_TrackFragmentBox *traf;
	GF_SubSampleInformationBox *subs = NULL;
	if (!movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	traf = gf_isom_get_traf(movie, TrackID);
	if (!traf || !traf->tfhd->sample_desc_index) return GF_BAD_PARAM;

	/*compute last sample number in traf*/
	last_sample = 0;
	count = gf_list_count(traf->TrackRuns);
	for (i=0; i<count; i++) {
		GF_TrackFragmentRunBox *trun = (GF_TrackFragmentRunBox*)gf_list_get(traf->TrackRuns, i);
		last_sample += trun->sample_count;
	}

	if (!traf->sub_samples) {
		traf->sub_samples = gf_list_new();
	}
	count = gf_list_count(traf->sub_samples);
	for (i=0; i<count;i++) {
		subs = gf_list_get(traf->sub_samples, i);
		if (subs->flags==flags) break;
		subs=NULL;
	}
	if (!subs) {
		subs = (GF_SubSampleInformationBox *) gf_isom_box_new_parent(&traf->child_boxes, GF_ISOM_BOX_TYPE_SUBS);
		if (!subs) return GF_OUT_OF_MEM;
		subs->version = (subSampleSize>0xFFFF) ? 1 : 0;
		subs->flags = flags;
		gf_list_add(traf->sub_samples, subs);
	}
	return gf_isom_add_subsample_info(subs, last_sample, subSampleSize, priority, reserved, discardable);
}

#if 0 //unused
static GF_Err gf_isom_copy_sample_group_entry_to_traf(GF_TrackFragmentBox *traf, GF_SampleTableBox *stbl, u32 grouping_type, u32 grouping_type_parameter, u32 sampleGroupDescriptionIndex, Bool sgpd_in_traf)
{
	if (sgpd_in_traf) {
		void *entry = NULL;
		u32 i, count;
		GF_SampleGroupDescriptionBox *sgdesc = NULL;
		GF_BitStream *bs;

		count = gf_list_count(stbl->sampleGroupsDescription);
		for (i = 0; i < count; i++) {
			sgdesc = (GF_SampleGroupDescriptionBox *)gf_list_get(stbl->sampleGroupsDescription, i);
			if (sgdesc->grouping_type == grouping_type)
				break;
			sgdesc = NULL;
		}
		if (!sgdesc)
			return GF_BAD_PARAM;

		entry = gf_list_get(sgdesc->group_descriptions, sampleGroupDescriptionIndex-1);
		if (!entry)
			return GF_BAD_PARAM;

		switch (grouping_type) {
		case GF_ISOM_SAMPLE_GROUP_RAP:
		{
			char udta[2];
			bs = gf_bs_new(udta, 2*sizeof(char), GF_BITSTREAM_WRITE);
			gf_bs_write_u8(bs, ((GF_VisualRandomAccessEntry *)entry)->num_leading_samples_known);
			gf_bs_write_u8(bs, ((GF_VisualRandomAccessEntry *)entry)->num_leading_samples);
			gf_bs_del(bs);
			return gf_isom_set_sample_group_info_ex(NULL, traf, 0, grouping_type, 0, udta, sg_rap_create_entry, sg_rap_compare_entry);
		}
		case GF_ISOM_SAMPLE_GROUP_SYNC:
		{
			char udta[1];
			bs = gf_bs_new(udta, 1*sizeof(char), GF_BITSTREAM_WRITE);
			gf_bs_write_int(bs, 0, 2);
			gf_bs_write_int(bs, ((GF_SYNCEntry *)entry)->NALU_type, 6);
			gf_bs_del(bs);
			return gf_isom_set_sample_group_info_ex(NULL, traf, 0, grouping_type, 0, udta, sg_rap_create_entry, sg_rap_compare_entry);
		}
		case GF_ISOM_SAMPLE_GROUP_ROLL:
		{
			char udta[2];
			bs = gf_bs_new(udta, 2*sizeof(char), GF_BITSTREAM_WRITE);
			gf_bs_write_u16(bs, ((GF_RollRecoveryEntry *)entry)->roll_distance);
			gf_bs_del(bs);
			return gf_isom_set_sample_group_info_ex(NULL, traf, 0, grouping_type, 0, udta, sg_roll_create_entry, sg_roll_compare_entry);
		}
		case GF_ISOM_SAMPLE_GROUP_SEIG:
		{
			return gf_isom_set_sample_group_info_ex(NULL, traf, 0, grouping_type, 0, entry, sg_encryption_create_entry, sg_encryption_compare_entry);
		}
		default:
			return GF_BAD_PARAM;
		}
	}

	return gf_isom_add_sample_group_entry(traf->sampleGroups, 0, grouping_type, grouping_type_parameter, sampleGroupDescriptionIndex, NULL);
}
/*copy over the subsample and sampleToGroup information of the given sample from the source track/file to the last sample added to the current track fragment of the destination file*/
GF_Err gf_isom_fragment_copy_subsample(GF_ISOFile *dest, GF_ISOTrackID TrackID, GF_ISOFile *orig, u32 track, u32 sampleNumber, Bool sgpd_in_traf)
{
	u32 i, count, last_sample, idx, subs_flags;
	GF_SubSampleInfoEntry *sub_sample;
	GF_Err e;
	GF_TrackBox *trak;
	GF_TrackFragmentBox *traf;
	GF_TrunEntry *ent;
	GF_TrackFragmentRunBox *trun;
	if (!dest->moof || !(dest->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	traf = gf_isom_get_traf(dest, TrackID);
	if (!traf || !traf->tfhd->sample_desc_index) return GF_BAD_PARAM;

	trak = gf_isom_get_track_from_file(orig, track);
	if (!trak) return GF_BAD_PARAM;

	/*modify depends flags*/
	if (trak->Media->information->sampleTable->SampleDep) {
		u32 isLeading, dependsOn, dependedOn, redundant;

		isLeading = dependsOn = dependedOn = redundant = 0;
		count = gf_list_count(traf->TrackRuns);
		if (!count) return GF_BAD_PARAM;
		trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
		count = gf_list_count(trun->entries);
		if (!count) return GF_BAD_PARAM;

		ent = (GF_TrunEntry *)gf_list_get(trun->entries, count-1);
		e = stbl_GetSampleDepType(trak->Media->information->sampleTable->SampleDep, sampleNumber, &isLeading, &dependsOn, &dependedOn, &redundant);
		if (e) return e;

		GF_ISOM_RESET_FRAG_DEPEND_FLAGS(ent->flags);

		if (traf->use_sdtp) {
			u8 sflags=0;
			if (!traf->sdtp) {
				traf->sdtp = (GF_SampleDependencyTypeBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SDTP);
				if (!traf->sdtp) return GF_OUT_OF_MEM;
			}
			sflags |= isLeading << 6;
			sflags |= dependsOn << 4;
			sflags |= dependedOn << 2;
			sflags |= redundant;

			traf->sdtp->sample_info = gf_realloc(traf->sdtp->sample_info, sizeof(u8)*(traf->sdtp->sampleCount+1));
			traf->sdtp->sample_info[traf->sdtp->sampleCount] = (u8) sflags;
			traf->sdtp->sampleCount++;
			traf->sdtp->sample_alloc = traf->sdtp->sampleCount+1;


			if (traf->use_sdtp==2) {
				ent->flags |= GF_ISOM_GET_FRAG_DEPEND_FLAGS(isLeading, dependsOn, dependedOn, redundant);
			}
		} else {
			ent->flags |= GF_ISOM_GET_FRAG_DEPEND_FLAGS(isLeading, dependsOn, dependedOn, redundant);
		}
	}

	/*copy subsample info if any*/
	idx=1;
	while (gf_isom_get_subsample_types(orig, track, idx, &subs_flags)) {
		GF_SubSampleInformationBox *subs_traf=NULL;
		idx++;
		if (! gf_isom_sample_get_subsample_entry(orig, track, sampleNumber, subs_flags, &sub_sample))
			continue;

		if (!traf || !traf->tfhd->sample_desc_index) return GF_BAD_PARAM;

		/*compute last sample number in traf*/
		last_sample = 0;
		count = gf_list_count(traf->TrackRuns);
		for (i=0; i<count; i++) {
			GF_TrackFragmentRunBox *trun = (GF_TrackFragmentRunBox*)gf_list_get(traf->TrackRuns, i);
			last_sample += trun->sample_count;
		}

		/*create subsample if needed*/
		if (!traf->sub_samples) {
			traf->sub_samples = gf_list_new();
		}
		count = gf_list_count(traf->sub_samples);
		for (i=0; i<count; i++) {
			subs_traf = gf_list_get(traf->sub_samples, i);
			if (subs_traf->flags==subs_flags) break;
			subs_traf = NULL;
		}
		if (!subs_traf) {
			subs_traf = (GF_SubSampleInformationBox *) gf_isom_box_new_parent(&traf->child_boxes, GF_ISOM_BOX_TYPE_SUBS);
			if (!subs_traf) return GF_OUT_OF_MEM;
			subs_traf->version = 0;
			subs_traf->flags = subs_flags;
			gf_list_add(traf->sub_samples, subs_traf);
		}

		count = gf_list_count(sub_sample->SubSamples);
		for (i=0; i<count; i++) {
			GF_SubSampleEntry *entry = (GF_SubSampleEntry*)gf_list_get(sub_sample->SubSamples, i);
			e = gf_isom_add_subsample_info(subs_traf, last_sample, entry->subsample_size, entry->subsample_priority, entry->reserved, entry->discardable);
			if (e) return e;
		}
	}
	/*copy sampleToGroup info if any*/
	if (trak->Media->information->sampleTable->sampleGroups) {
		count = gf_list_count(trak->Media->information->sampleTable->sampleGroups);
		for (i=0; i<count; i++) {
			GF_SampleGroupBox *sg;
			Bool found = GF_FALSE;
			u32 j;
			u32 first_sample_in_entry, last_sample_in_entry;
			first_sample_in_entry = 1;

			sg = (GF_SampleGroupBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroups, i);
			for (j=0; j<sg->entry_count; j++) {
				last_sample_in_entry = first_sample_in_entry + sg->sample_entries[j].sample_count - 1;
				if ((sampleNumber<first_sample_in_entry) || (sampleNumber>last_sample_in_entry)) {
					first_sample_in_entry = last_sample_in_entry+1;
					continue;
				}

				if (!traf->sampleGroups)
					traf->sampleGroups = gf_list_new();

				/*found our sample, add it to trak->sampleGroups*/
				e = gf_isom_copy_sample_group_entry_to_traf(traf, trak->Media->information->sampleTable, sg->grouping_type, sg->grouping_type_parameter,  sg->sample_entries[j].group_description_index, sgpd_in_traf);
				if (e) return e;

				found = GF_TRUE;
				break;
			}
			//unmapped sample
			if (!found) {
				if (!traf->sampleGroups)
					traf->sampleGroups = gf_list_new();

				e = gf_isom_copy_sample_group_entry_to_traf(traf, trak->Media->information->sampleTable, sg->grouping_type, sg->grouping_type_parameter,  0, sgpd_in_traf);
				if (e) return e;
			}
		}
	}
	return GF_OK;
}
#endif


GF_Err gf_isom_fragment_set_sample_flags(GF_ISOFile *movie, GF_ISOTrackID trackID, u32 is_leading, u32 dependsOn, u32 dependedOn, u32 redundant)
{
	u32 count;
	GF_TrackFragmentBox *traf;
	GF_TrunEntry *ent;
	GF_TrackFragmentRunBox *trun;
	if (!movie || !movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	traf = gf_isom_get_traf(movie, trackID);
	if (!traf->tfhd->sample_desc_index) return GF_BAD_PARAM;

	count = gf_list_count(traf->TrackRuns);
	if (!count) return GF_BAD_PARAM;
	trun = (GF_TrackFragmentRunBox *)gf_list_get(traf->TrackRuns, count-1);
	if (!trun->nb_samples) return GF_BAD_PARAM;
	ent = &trun->samples[trun->nb_samples-1];

	GF_ISOM_RESET_FRAG_DEPEND_FLAGS(ent->flags);

	if (traf->use_sdtp) {
		u8 sflags=0;
		if (!traf->sdtp) {
			traf->sdtp = (GF_SampleDependencyTypeBox *) gf_isom_box_new_parent(&traf->child_boxes, GF_ISOM_BOX_TYPE_SDTP);
			if (!traf->sdtp) return GF_OUT_OF_MEM;
		}
		sflags |= is_leading << 6;
		sflags |= dependsOn << 4;
		sflags |= dependedOn << 2;
		sflags |= redundant;

		traf->sdtp->sample_info = gf_realloc(traf->sdtp->sample_info, sizeof(u8)*(traf->sdtp->sampleCount+1));
		traf->sdtp->sample_info[traf->sdtp->sampleCount] = (u8) sflags;
		traf->sdtp->sampleCount++;
		traf->sdtp->sample_alloc = traf->sdtp->sampleCount;
		if (traf->use_sdtp==2) {
			ent->flags |= GF_ISOM_GET_FRAG_DEPEND_FLAGS(is_leading, dependsOn, dependedOn, redundant);
		}
	} else {
		ent->flags |= GF_ISOM_GET_FRAG_DEPEND_FLAGS(is_leading, dependsOn, dependedOn, redundant);
	}
	return GF_OK;
}


#endif	/*GPAC_DISABLE_ISOM_WRITE*/


GF_EXPORT
GF_Err gf_isom_set_traf_base_media_decode_time(GF_ISOFile *movie, GF_ISOTrackID TrackID, u64 decode_time)
{
	GF_TrackFragmentBox *traf;
	if (!movie || !movie->moof || !(movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) return GF_BAD_PARAM;

	traf = gf_isom_get_traf(movie, TrackID);
	if (!traf) return GF_BAD_PARAM;

	if (!traf->tfdt) {
		traf->tfdt = (GF_TFBaseMediaDecodeTimeBox *) gf_isom_box_new_parent(&traf->child_boxes, GF_ISOM_BOX_TYPE_TFDT);
		if (!traf->tfdt) return GF_OUT_OF_MEM;
	}
	traf->tfdt->baseMediaDecodeTime = decode_time;
	if (traf->large_tfdt)
		traf->tfdt->version = 1;
	return GF_OK;
}

GF_Err gf_isom_enable_mfra(GF_ISOFile *file)
{
	if (!file) return GF_BAD_PARAM;
	file->mfra = (GF_MovieFragmentRandomAccessBox *)gf_isom_box_new(GF_ISOM_BOX_TYPE_MFRA);
	if (!file->mfra) return GF_OUT_OF_MEM;
	return GF_OK;
}

#endif /*GPAC_DISABLE_ISOM_FRAGMENTS)*/


GF_EXPORT
void gf_isom_set_next_moof_number(GF_ISOFile *movie, u32 value)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (movie) movie->NextMoofNumber = value;
#endif
}

GF_EXPORT
u32 gf_isom_get_next_moof_number(GF_ISOFile *movie)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (movie) return movie->NextMoofNumber;
#endif
	return 0;
}

GF_Err gf_isom_set_emsg(GF_ISOFile *movie, u8 *data, u32 size)
{
	if (!movie || !data) return GF_BAD_PARAM;
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (!movie->moof) return GF_BAD_PARAM;

	GF_BitStream *bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	while (gf_bs_available(bs)) {
		GF_Box *emsg;
		GF_Err e = gf_isom_box_parse(&emsg, bs);
		if (e) break;

		if (!movie->moof->emsgs) movie->moof->emsgs = gf_list_new();
		gf_list_add(movie->moof->emsgs, emsg);
	}
	gf_bs_del(bs);
#endif
	return GF_OK;
}


GF_EXPORT
Bool gf_isom_is_track_fragmented(GF_ISOFile *movie, GF_ISOTrackID TrackID)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (!movie || !movie->moov || !movie->moov->mvex) return GF_FALSE;
	return (GetTrex(movie->moov, TrackID) != NULL) ? GF_TRUE : GF_FALSE;
#else
	return GF_FALSE;
#endif
}

GF_EXPORT
Bool gf_isom_is_fragmented(GF_ISOFile *movie)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (!movie || !movie->moov) return GF_FALSE;
	/* By default if the Moov has an mvex, the file is fragmented */
	if (movie->moov->mvex) return GF_TRUE;
#endif
	return GF_FALSE;
}

#endif /*GPAC_DISABLE_ISOM*/
