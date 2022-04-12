/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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
#include <gpac/network.h>
#include <gpac/thread.h>

#ifndef GPAC_DISABLE_ISOM

/**************************************************************
		Some Local functions for movie creation
**************************************************************/
GF_Err gf_isom_parse_root_box(GF_Box **outBox, GF_BitStream *bs, u32 *boxType, u64 *bytesExpected, Bool progressive_mode);

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
GF_Err MergeFragment(GF_MovieFragmentBox *moof, GF_ISOFile *mov)
{
	GF_Err e;
	u32 i, j;
	u64 MaxDur;
	GF_TrackFragmentBox *traf;
	GF_TrackBox *trak;
	u64 base_data_offset;

	MaxDur = 0;

	//we shall have a MOOV and its MVEX BEFORE any MOOF
	if (!mov->moov || !mov->moov->mvex) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Error: %s not received before merging fragment\n", mov->moov ? "mvex" : "moov" ));
		return GF_ISOM_INVALID_FILE;
	}
	//and all fragments should be continous but:
	//- dash with dependent representations may likely give R1(moofSN 1, 3, 5, 7) plus R2(moofSN 2, 4, 6, 8)
	//- smooth muxed in a single file may end up with V(1),A(1), V(2),A(2) ...
	//we do not throw an error if not as we may still want to be able to concatenate dependent representations in DASH and
	if (mov->NextMoofNumber && moof->mfhd && (mov->NextMoofNumber >= moof->mfhd->sequence_number)) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] wrong sequence number: got %d but last one was %d\n", moof->mfhd->sequence_number, mov->NextMoofNumber));
	}

	base_data_offset = mov->current_top_box_start;
	if (moof->compressed_diff)
		base_data_offset -= moof->compressed_diff;

	i=0;
	while ((traf = (GF_TrackFragmentBox*)gf_list_enum(moof->TrackList, &i))) {
		u32 prev_sample_count;
		if (!traf->tfhd) {
			trak = NULL;
			traf->trex = NULL;
		} else if (mov->is_smooth) {
			trak = gf_list_get(mov->moov->trackList, 0);
			traf->trex = (GF_TrackExtendsBox*)gf_list_get(mov->moov->mvex->TrackExList, 0);
			assert(traf->trex);
			traf->trex->trackID = trak->Header->trackID = traf->tfhd->trackID;
		} else {
			trak = gf_isom_get_track_from_id(mov->moov, traf->tfhd->trackID);
			j=0;
			while ((traf->trex = (GF_TrackExtendsBox*)gf_list_enum(mov->moov->mvex->TrackExList, &j))) {
				if (traf->trex->trackID == traf->tfhd->trackID) break;
				traf->trex = NULL;
			}
		}

		if (!trak || !traf->trex) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Error: Cannot find fragment track with ID %d\n", traf->tfhd ? traf->tfhd->trackID : 0));
			return GF_ISOM_INVALID_FILE;
		}

		prev_sample_count = trak->Media->information->sampleTable->SampleSize ? trak->Media->information->sampleTable->SampleSize->sampleCount : 0;
		e = MergeTrack(trak, traf, moof, mov->current_top_box_start, moof->compressed_diff, &base_data_offset);
		if (e) return e;
		trak->first_traf_merged = GF_TRUE;

		trak->present_in_scalable_segment = 1;

		//update trak duration
		SetTrackDuration(trak);
		if (trak->Header->duration > MaxDur)
			MaxDur = trak->Header->duration;

		//we have PSSH per moov, internally remap as a sample group of type PSSH
		if (gf_list_count(moof->PSSHs)) {
			u8 *pssh_data;
			u32 pssh_len;
			u32 j, k, nb_pssh = gf_list_count(moof->PSSHs);
			GF_BitStream *pssh_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			gf_bs_write_u32(pssh_bs, nb_pssh);
			for (j=0; j<nb_pssh; j++) {
				GF_ProtectionSystemHeaderBox *pssh = gf_list_get(moof->PSSHs, j);
				gf_bs_write_data(pssh_bs, pssh->SystemID, 16);
				gf_bs_write_u32(pssh_bs, pssh->version);
				if (pssh->version) {
					gf_bs_write_u32(pssh_bs, pssh->KID_count);
					for (k=0; k<pssh->KID_count; k++) {
						gf_bs_write_data(pssh_bs, pssh->KIDs[k], 16);
					}
				}
				gf_bs_write_u32(pssh_bs, pssh->private_data_size);
				gf_bs_write_data(pssh_bs, pssh->private_data, pssh->private_data_size);
			}
			gf_bs_get_content(pssh_bs, &pssh_data, &pssh_len);
			gf_bs_del(pssh_bs);

			gf_isom_set_sample_group_description_internal(mov, gf_list_find(mov->moov->trackList, trak)+1, 1+prev_sample_count, GF_4CC('P','S','S','H'), 0, pssh_data, pssh_len, GF_FALSE);
			gf_free(pssh_data);
		}


		//we have emsg, internally remap as a sample group of type EMSG
		if (gf_list_count(mov->emsgs)) {
			u8 *emsg_data;
			u32 emsg_len;
			u32 j, nb_emsg = gf_list_count(mov->emsgs);
			GF_BitStream *emsg_bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
			for (j=0; j<nb_emsg; j++) {
				GF_Box *emsg = gf_list_get(mov->emsgs, j);
				gf_isom_box_write(emsg, emsg_bs);
			}
			gf_bs_get_content(emsg_bs, &emsg_data, &emsg_len);
			gf_bs_del(emsg_bs);

			gf_isom_set_sample_group_description_internal(mov, gf_list_find(mov->moov->trackList, trak)+1, 1+prev_sample_count, GF_4CC('E','M','S','G'), 0, emsg_data, emsg_len, GF_FALSE);
			gf_free(emsg_data);
		}
	}
	if (mov->emsgs) {
		gf_isom_box_array_del(mov->emsgs);
		mov->emsgs = NULL;
	}

	if (moof->child_boxes) {
		GF_Box *a;
		i = 0;
		while ((a = (GF_Box *)gf_list_enum(moof->child_boxes, &i))) {
			if (a->type == GF_ISOM_BOX_TYPE_PSSH) {
				GF_ProtectionSystemHeaderBox *pssh = (GF_ProtectionSystemHeaderBox *)gf_isom_box_new_parent(&mov->moov->child_boxes, GF_ISOM_BOX_TYPE_PSSH);
				if (!pssh) return GF_OUT_OF_MEM;
				memmove(pssh->SystemID, ((GF_ProtectionSystemHeaderBox *)a)->SystemID, 16);
				if (((GF_ProtectionSystemHeaderBox *)a)->KIDs && ((GF_ProtectionSystemHeaderBox *)a)->KID_count > 0) {
					pssh->KID_count = ((GF_ProtectionSystemHeaderBox *)a)->KID_count;
					pssh->KIDs = (bin128 *)gf_malloc(pssh->KID_count*sizeof(bin128));
					if (!pssh->KIDs) return GF_OUT_OF_MEM;

					memmove(pssh->KIDs, ((GF_ProtectionSystemHeaderBox *)a)->KIDs, pssh->KID_count*sizeof(bin128));
				}
				pssh->private_data_size = ((GF_ProtectionSystemHeaderBox *)a)->private_data_size;
				pssh->private_data = (u8 *)gf_malloc(pssh->private_data_size*sizeof(char));
				if (!pssh->private_data) return GF_OUT_OF_MEM;
				memmove(pssh->private_data, ((GF_ProtectionSystemHeaderBox *)a)->private_data, pssh->private_data_size);
				pssh->moof_defined = 1;
				mov->has_pssh_moof = GF_TRUE;
			}
		}
	}

	mov->NextMoofNumber = moof->mfhd ? moof->mfhd->sequence_number : 0;
	//update movie duration
	if (mov->moov->mvhd->duration < MaxDur) mov->moov->mvhd->duration = MaxDur;
	return GF_OK;
}

static void FixTrackID(GF_ISOFile *mov)
{
	if (!mov->moov) return;

	if (gf_list_count(mov->moov->trackList) == 1 && gf_list_count(mov->moof->TrackList) == 1) {
		GF_TrackFragmentBox *traf = (GF_TrackFragmentBox*)gf_list_get(mov->moof->TrackList, 0);
		GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(mov->moov->trackList, 0);
		if (!traf->tfhd || !trak->Header) return;
		if ((traf->tfhd->trackID != trak->Header->trackID)) {
			if (!mov->is_smooth) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Warning: trackID of MOOF/TRAF(%u) is not the same as MOOV/TRAK(%u). Trying to fix.\n", traf->tfhd->trackID, trak->Header->trackID));
			} else {
				trak->Header->trackID = traf->tfhd->trackID;
			}
			traf->tfhd->trackID = trak->Header->trackID;
		}
	}
}

static void FixSDTPInTRAF(GF_MovieFragmentBox *moof)
{
	u32 k;
	if (!moof)
		return;

	for (k = 0; k < gf_list_count(moof->TrackList); k++) {
		GF_TrackFragmentBox *traf = gf_list_get(moof->TrackList, k);
		if (traf->sdtp) {
			GF_TrackFragmentRunBox *trun;
			u32 j = 0, sample_index = 0;

			if (traf->sdtp->sampleCount == gf_list_count(traf->TrackRuns)) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Warning: TRAF box of track id=%u contains a SDTP. Converting to TRUN sample flags.\n", traf->tfhd->trackID));
			}

			while ((trun = (GF_TrackFragmentRunBox*)gf_list_enum(traf->TrackRuns, &j))) {
				u32 i;
				trun->flags |= GF_ISOM_TRUN_FLAGS;
				for (i=0; i<trun->nb_samples; i++) {
					GF_TrunEntry *entry = &trun->samples[i];
					const u8 info = traf->sdtp->sample_info[sample_index];
					entry->flags |= GF_ISOM_GET_FRAG_DEPEND_FLAGS(info >> 6, info >> 4, info >> 2, info);
					sample_index++;
					if (sample_index > traf->sdtp->sampleCount) {
						GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Error: TRAF box of track id=%u contained an inconsistent SDTP.\n", traf->tfhd->trackID));
						return;
					}
				}
			}
			if (sample_index < traf->sdtp->sampleCount) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Error: TRAF box of track id=%u list less samples than SDTP.\n", traf->tfhd->trackID));
			}
			gf_isom_box_del_parent(&traf->child_boxes, (GF_Box*)traf->sdtp);
			traf->sdtp = NULL;
		}
	}
}

void gf_isom_push_mdat_end(GF_ISOFile *mov, u64 mdat_end)
{
	u32 i, count;
	if (!mov || !mov->moov) return;

	count = gf_list_count(mov->moov->trackList);
	for (i=0; i<count; i++) {
		u32 j;
		GF_TrafToSampleMap *traf_map;
		GF_TrackBox *trak = gf_list_get(mov->moov->trackList, i);
		if (!trak->Media->information->sampleTable->traf_map) continue;

		traf_map = trak->Media->information->sampleTable->traf_map;
		for (j=traf_map->nb_entries; j>0; j--) {
			if (!traf_map->frag_starts[j-1].mdat_end) {
				traf_map->frag_starts[j-1].mdat_end = mdat_end;
				break;
			}
		}
	}
}

#ifdef GF_ENABLE_CTRN
static void gf_isom_setup_traf_inheritance(GF_ISOFile *mov)
{
	u32 i, count;
	if (!mov->moov->mvex)
		return;
	count = gf_list_count(mov->moov->trackList);

	for (i=0; i<count; i++) {
		u32 refTrackNum=0;
		gf_isom_get_reference(mov, i+1, GF_ISOM_REF_TRIN, 1, &refTrackNum);
		if (refTrackNum) {
			GF_ISOTrackID tkid = gf_isom_get_track_id(mov, i+1);
			GF_ISOTrackID reftkid = gf_isom_get_track_id(mov, refTrackNum);
			GF_TrackExtendsBox *trex = GetTrex(mov->moov, tkid);
			if (trex) trex->inherit_from_traf_id = reftkid;
		}
	}
}
#endif

#endif

//for now we only use regular sample to group internally (except when dumping), not the pattern version
//we unrill the pattern and replace the compact version with a regular one
static void convert_compact_sample_groups(GF_List *child_boxes, GF_List *sampleGroups)
{
	u32 i;
	for (i=0; i<gf_list_count(sampleGroups); i++) {
		u32 j;
		GF_SampleGroupBox *sbgp;
		GF_CompactSampleGroupBox *csgp = gf_list_get(sampleGroups, i);
		if (csgp->type != GF_ISOM_BOX_TYPE_CSGP) continue;

		gf_list_rem(sampleGroups, i);
		gf_list_del_item(child_boxes, csgp);

		sbgp = (GF_SampleGroupBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_SBGP);
		gf_list_insert(sampleGroups, sbgp, i);
		gf_list_add(child_boxes, sbgp);
		i--;

		sbgp->grouping_type = csgp->grouping_type;
		if (csgp->grouping_type_parameter) {
			sbgp->grouping_type_parameter = csgp->grouping_type_parameter;
			sbgp->version = 1;
		}
		sbgp->entry_count = 0;
		for (j=0; j<csgp->pattern_count; j++) {
			u32 k=0;
			u32 nb_samples = csgp->patterns[j].sample_count;
			//unroll the pattern
			while (nb_samples) {
				u32 nb_same_index=1;
				if (csgp->patterns[j].length<=k)
					break;
				u32 sg_idx = csgp->patterns[j].sample_group_description_indices[k];
				while (nb_same_index+k<csgp->patterns[j].length) {
					if (csgp->patterns[j].sample_group_description_indices[k+nb_same_index] != sg_idx)
						break;
					nb_same_index++;
				}
				sbgp->sample_entries = gf_realloc(sbgp->sample_entries, sizeof(GF_SampleGroupEntry) * (sbgp->entry_count+1));
				if (nb_same_index>nb_samples)
					nb_same_index = nb_samples;

				sbgp->sample_entries[sbgp->entry_count].sample_count = nb_same_index;
				sbgp->sample_entries[sbgp->entry_count].group_description_index = sg_idx;
				nb_samples -= nb_same_index;
				sbgp->entry_count++;
				k+= nb_same_index;
				if (k==csgp->patterns[j].length)
					k = 0;
			}
		}
		gf_isom_box_del((GF_Box*)csgp);
	}
}


static GF_Err gf_isom_parse_movie_boxes_internal(GF_ISOFile *mov, u32 *boxType, u64 *bytesMissing, Bool progressive_mode)
{
	GF_Box *a;
	u64 totSize, mdat_end=0;
	GF_Err e = GF_OK;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (mov->single_moof_mode && mov->single_moof_state == 2) {
		return e;
	}

	/*restart from where we stopped last*/
	totSize = mov->current_top_box_start;
	if (mov->bytes_removed) {
		assert(totSize >= mov->bytes_removed);
		totSize -= mov->bytes_removed;
	}
	gf_bs_seek(mov->movieFileMap->bs, totSize);
#endif


	/*while we have some data, parse our boxes*/
	while (gf_bs_available(mov->movieFileMap->bs)) {
		*bytesMissing = 0;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		mov->current_top_box_start = gf_bs_get_position(mov->movieFileMap->bs) + mov->bytes_removed;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CONTAINER, ("[iso file] Parsing a top-level box at position %d\n", mov->current_top_box_start));
#endif

		e = gf_isom_parse_root_box(&a, mov->movieFileMap->bs, boxType, bytesMissing, progressive_mode);

		if (e >= 0) {
			//safety check, should never happen
			if (!a) return GF_ISOM_INVALID_FILE;
		} else if (e == GF_ISOM_INCOMPLETE_FILE) {
			/*our mdat is uncomplete, only valid for READ ONLY files...*/
			if (mov->openMode != GF_ISOM_OPEN_READ) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Incomplete MDAT while file is not read-only\n"));
				return GF_ISOM_INVALID_FILE;
			}
			if ((mov->openMode == GF_ISOM_OPEN_READ) && !progressive_mode) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Incomplete file while reading for dump - aborting parsing\n"));
				break;
			}
			return e;
		} else {
			return e;
		}

		switch (a->type) {
		/*MOOV box*/
		case GF_ISOM_BOX_TYPE_MOOV:
			if (mov->moov) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Duplicate MOOV detected!\n"));
				gf_isom_box_del(a);
				return GF_ISOM_INVALID_FILE;
			}
			mov->moov = (GF_MovieBox *)a;
			mov->original_moov_offset = mov->current_top_box_start;
			/*set our pointer to the movie*/
			mov->moov->mov = mov;
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			if (mov->moov->mvex) mov->moov->mvex->mov = mov;

#ifdef GF_ENABLE_CTRN
			if (! (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG)) {
				gf_isom_setup_traf_inheritance(mov);
			}
#endif

#endif
			e = gf_list_add(mov->TopBoxes, a);
			if (e) return e;

			totSize += a->size;

            if (!mov->moov->mvhd) {
                GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing MovieHeaderBox\n"));
                return GF_ISOM_INVALID_FILE;
            }

            if (mov->meta) {
				gf_isom_meta_restore_items_ref(mov, mov->meta);
			}

			//dump senc info in dump mode
			if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) {
				u32 k;
				for (k=0; k<gf_list_count(mov->moov->trackList); k++) {
					GF_TrackBox *trak = (GF_TrackBox *)gf_list_get(mov->moov->trackList, k);

					if (trak->sample_encryption) {
						e = senc_Parse(mov->movieFileMap->bs, trak, NULL, trak->sample_encryption);
						if (e) return e;
					}
				}
			} else {
				u32 k;
				for (k=0; k<gf_list_count(mov->moov->trackList); k++) {
					GF_TrackBox *trak = (GF_TrackBox *)gf_list_get(mov->moov->trackList, k);
					if (trak->Media->information->sampleTable->sampleGroups) {
						convert_compact_sample_groups(trak->Media->information->sampleTable->child_boxes, trak->Media->information->sampleTable->sampleGroups);
					}
				}
			}

            if (mdat_end && mov->signal_frag_bounds && !(mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) ) {
                gf_isom_push_mdat_end(mov, mdat_end);
                mdat_end=0;
            }
			break;

		/*META box*/
		case GF_ISOM_BOX_TYPE_META:
			if (mov->meta) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Duplicate META detected!\n"));
				gf_isom_box_del(a);
				return GF_ISOM_INVALID_FILE;
			}
			mov->meta = (GF_MetaBox *)a;
			mov->original_meta_offset = mov->current_top_box_start;
			e = gf_list_add(mov->TopBoxes, a);
			if (e) {
				return e;
			}
			totSize += a->size;
			gf_isom_meta_restore_items_ref(mov, mov->meta);
			break;

		/*we only keep the MDAT in READ for dump purposes*/
		case GF_ISOM_BOX_TYPE_MDAT:
			if (!mov->first_data_toplevel_offset) {
				mov->first_data_toplevel_offset = mov->current_top_box_start;
				mov->first_data_toplevel_size = a->size;
			}
			totSize += a->size;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
			if (mov->emsgs) {
				gf_isom_box_array_del(mov->emsgs);
				mov->emsgs = NULL;
			}
#endif

			if (mov->openMode == GF_ISOM_OPEN_READ) {
				if (!mov->mdat) {
					mov->mdat = (GF_MediaDataBox *) a;
					e = gf_list_add(mov->TopBoxes, mov->mdat);
					if (e) {
						return e;
					}
				}
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
				else if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) gf_list_add(mov->TopBoxes, a);
#endif
				else gf_isom_box_del(a); //in other modes we don't care


				if (mov->signal_frag_bounds && !(mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) ) {
                    mdat_end = gf_bs_get_position(mov->movieFileMap->bs);
                    if (mov->moov) {
                        gf_isom_push_mdat_end(mov, mdat_end);
                        mdat_end=0;
                    }
				}
			}
			/*if we don't have any MDAT yet, create one (edit-write mode)
			We only work with one mdat, but we're puting it at the place
			of the first mdat found when opening a file for editing*/
			else if (!mov->mdat && (mov->openMode != GF_ISOM_OPEN_READ) && (mov->openMode != GF_ISOM_OPEN_KEEP_FRAGMENTS)) {
				gf_isom_box_del(a);
				mov->mdat = (GF_MediaDataBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MDAT);
				if (!mov->mdat) return GF_OUT_OF_MEM;
				e = gf_list_add(mov->TopBoxes, mov->mdat);
				if (e) {
					return e;
				}
			} else {
				gf_isom_box_del(a);
			}
			break;
		case GF_ISOM_BOX_TYPE_FTYP:
			/*ONE AND ONLY ONE FTYP*/
			if (mov->brand) {
				gf_isom_box_del(a);
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Duplicate 'ftyp' detected!\n"));
				return GF_ISOM_INVALID_FILE;
			}
			mov->brand = (GF_FileTypeBox *)a;
			totSize += a->size;
			e = gf_list_add(mov->TopBoxes, a);
			if (e) return e;
			break;

		case GF_ISOM_BOX_TYPE_OTYP:
			/*ONE AND ONLY ONE FTYP*/
			if (mov->otyp) {
				gf_isom_box_del(a);
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Duplicate 'otyp' detected!\n"));
				return GF_ISOM_INVALID_FILE;
			}

			if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) {
				mov->otyp = (GF_Box *)a;
				totSize += a->size;
				e = gf_list_add(mov->TopBoxes, a);
				if (e) return e;
			} else {
				GF_FileTypeBox *brand = (GF_FileTypeBox *) gf_isom_box_find_child(a->child_boxes, GF_ISOM_BOX_TYPE_FTYP);
				if (brand) {
					s32 pos;
					gf_list_del_item(a->child_boxes, brand);
					pos = gf_list_del_item(mov->TopBoxes, mov->brand);
					gf_isom_box_del((GF_Box *) mov->brand);
					mov->brand = brand;
					if (pos<0) pos=0;
					gf_list_insert(mov->TopBoxes, brand, pos);
				}
				gf_isom_box_del(a);
			}
			break;

		case GF_ISOM_BOX_TYPE_PDIN:
			/*ONE AND ONLY ONE PDIN*/
			if (mov->pdin) {
				gf_isom_box_del(a);
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Duplicate 'pdin'' detected!\n"));
				return GF_ISOM_INVALID_FILE;
			}
			mov->pdin = (GF_ProgressiveDownloadBox *) a;
			totSize += a->size;
			e = gf_list_add(mov->TopBoxes, a);
			if (e) return e;
			break;


#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		case GF_ISOM_BOX_TYPE_STYP:
		{
			u32 brand = ((GF_FileTypeBox *)a)->majorBrand;
			switch (brand) {
			case GF_ISOM_BRAND_SISX:
			case GF_ISOM_BRAND_RISX:
			case GF_ISOM_BRAND_SSSS:
				mov->is_index_segment = GF_TRUE;
				break;
			default:
				break;
			}
		}
		/*fall-through*/

		case GF_ISOM_BOX_TYPE_SIDX:
		case GF_ISOM_BOX_TYPE_SSIX:
			if (mov->moov && !mov->first_data_toplevel_offset) {
				mov->first_data_toplevel_offset = mov->current_top_box_start;
				mov->first_data_toplevel_size = a->size;
			}
			totSize += a->size;
			if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) {
				e = gf_list_add(mov->TopBoxes, a);
				if (e) return e;
			} else if (mov->signal_frag_bounds && !(mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG)  && (mov->openMode!=GF_ISOM_OPEN_KEEP_FRAGMENTS)
			) {
				if (a->type==GF_ISOM_BOX_TYPE_SIDX) {
					if (mov->root_sidx) gf_isom_box_del( (GF_Box *) mov->root_sidx);
					mov->root_sidx = (GF_SegmentIndexBox *) a;
					mov->sidx_start_offset = mov->current_top_box_start;
					mov->sidx_end_offset = gf_bs_get_position(mov->movieFileMap->bs);

				}
				else if (a->type==GF_ISOM_BOX_TYPE_STYP) {
					mov->styp_start_offset = mov->current_top_box_start;

					if (mov->seg_styp) gf_isom_box_del(mov->seg_styp);
					mov->seg_styp = a;
				} else if (a->type==GF_ISOM_BOX_TYPE_SSIX) {
					if (mov->seg_ssix) gf_isom_box_del(mov->seg_ssix);
					mov->seg_ssix = a;
				} else {
					gf_isom_box_del(a);
				}
				gf_isom_push_mdat_end(mov, mov->current_top_box_start);
			} else if (!mov->NextMoofNumber && (a->type==GF_ISOM_BOX_TYPE_SIDX)) {
				if (mov->main_sidx) gf_isom_box_del( (GF_Box *) mov->main_sidx);
				mov->main_sidx = (GF_SegmentIndexBox *) a;
				mov->main_sidx_end_pos = mov->current_top_box_start + a->size;
			} else {
				gf_isom_box_del(a);
			}
			break;

		case GF_ISOM_BOX_TYPE_MOOF:
			//no support for inplace rewrite for fragmented files
			gf_isom_disable_inplace_rewrite(mov);
			if (!mov->moov) {
				GF_LOG(mov->moof ? GF_LOG_DEBUG : GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Movie fragment but no moov (yet) - possibly broken parsing!\n"));
			}
			if (mov->single_moof_mode) {
				mov->single_moof_state++;
				if (mov->single_moof_state > 1) {
					gf_isom_box_del(a);
					return GF_OK;
				}
			}
			((GF_MovieFragmentBox *)a)->mov = mov;

			totSize += a->size;
			mov->moof = (GF_MovieFragmentBox *) a;

			/*some smooth streaming streams contain a SDTP under the TRAF: this is incorrect, convert it*/
			FixTrackID(mov);
			if (! (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG)) {
				FixSDTPInTRAF(mov->moof);
			} else {
				u32 k;
				for (k=0; k<gf_list_count(mov->moof->TrackList); k++) {
					GF_TrackFragmentBox *traf = (GF_TrackFragmentBox *)gf_list_get(mov->moof->TrackList, k);
					if (traf->sampleGroups) {
						convert_compact_sample_groups(traf->child_boxes, traf->sampleGroups);
					}
				}
			}

			/*read & debug: store at root level*/
			if (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG) {
				u32 k;
				gf_list_add(mov->TopBoxes, a);
				/*also update pointers to trex for debug*/
				if (mov->moov) {
					for (k=0; k<gf_list_count(mov->moof->TrackList); k++) {
						GF_TrackFragmentBox *traf = gf_list_get(mov->moof->TrackList, k);
						if (traf->tfhd && mov->moov->mvex && mov->moov->mvex->TrackExList) {
							GF_TrackBox *trak = gf_isom_get_track_from_id(mov->moov, traf->tfhd->trackID);
							u32 j=0;
							while ((traf->trex = (GF_TrackExtendsBox*)gf_list_enum(mov->moov->mvex->TrackExList, &j))) {
								if (traf->trex->trackID == traf->tfhd->trackID) {
									if (!traf->trex->track) traf->trex->track = trak;
									break;
								}
								traf->trex = NULL;
							}
						}
						//we should only parse senc/psec when no saiz/saio is present, otherwise we fetch the info directly
						if (traf->trex && traf->tfhd && traf->trex->track && traf->sample_encryption) {
							GF_TrackBox *trak = GetTrackbyID(mov->moov, traf->tfhd->trackID);
							if (trak) {
								trak->current_traf_stsd_idx = traf->tfhd->sample_desc_index ? traf->tfhd->sample_desc_index : traf->trex->def_sample_desc_index;
								e = senc_Parse(mov->movieFileMap->bs, trak, traf, traf->sample_encryption);
								if (e) return e;
								trak->current_traf_stsd_idx = 0;
							}
						}
					}
				} else {
					for (k=0; k<gf_list_count(mov->moof->TrackList); k++) {
						GF_TrackFragmentBox *traf = gf_list_get(mov->moof->TrackList, k);
						if (traf->sample_encryption) {
							e = senc_Parse(mov->movieFileMap->bs, NULL, traf, traf->sample_encryption);
							if (e) return e;
						}
					}

				}
			} else if (mov->openMode==GF_ISOM_OPEN_KEEP_FRAGMENTS) {
				mov->NextMoofNumber = mov->moof->mfhd->sequence_number+1;
				mov->moof = NULL;
				gf_isom_box_del(a);
			} else {
				/*merge all info*/
				e = MergeFragment((GF_MovieFragmentBox *)a, mov);
				gf_isom_box_del(a);
				if (e) return e;
			}

			//done with moov
			if (mov->root_sidx) {
				gf_isom_box_del((GF_Box *) mov->root_sidx);
				mov->root_sidx = NULL;
			}
			if (mov->root_ssix) {
				gf_isom_box_del(mov->seg_ssix);
				mov->root_ssix = NULL;
			}
			if (mov->seg_styp) {
				gf_isom_box_del(mov->seg_styp);
				mov->seg_styp = NULL;
			}
			mov->sidx_start_offset = 0;
			mov->sidx_end_offset = 0;
			mov->styp_start_offset = 0;
			break;
#endif
		case GF_ISOM_BOX_TYPE_UNKNOWN:
		{
			GF_UnknownBox *box = (GF_UnknownBox*)a;
			if (box->original_4cc == GF_ISOM_BOX_TYPE_JP) {
				u8 *c = (u8 *) box->data;
				if ((box->dataSize==4) && (GF_4CC(c[0],c[1],c[2],c[3])==(u32)0x0D0A870A))
					mov->is_jp2 = 1;
				gf_isom_box_del(a);
			} else {
				e = gf_list_add(mov->TopBoxes, a);
				if (e) return e;
			}
		}
		break;

		case GF_ISOM_BOX_TYPE_PRFT:
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			if (!(mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG)) {
				//keep the last one read
				if (mov->last_producer_ref_time)
					gf_isom_box_del(a);
				else
					mov->last_producer_ref_time = (GF_ProducerReferenceTimeBox *)a;
				break;
			}
#endif
		//fallthrough
		case GF_ISOM_BOX_TYPE_EMSG:
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			if (! (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG)) {
				if (!mov->emsgs) mov->emsgs = gf_list_new();
				gf_list_add(mov->emsgs, a);
				break;
			}
#endif
		case GF_ISOM_BOX_TYPE_MFRA:
		case GF_ISOM_BOX_TYPE_MFRO:
			//only keep for dump mode, otherwise we ignore these boxes and we don't want to carry them over in non-fragmented file
			if (! (mov->FragmentsFlags & GF_ISOM_FRAG_READ_DEBUG)) {
				totSize += a->size;
				gf_isom_box_del(a);
				break;
			}
		default:
			totSize += a->size;
			e = gf_list_add(mov->TopBoxes, a);
			if (e) return e;
			break;
		}

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		/*remember where we left, in case we append an entire number of movie fragments*/
		mov->current_top_box_start = gf_bs_get_position(mov->movieFileMap->bs) + mov->bytes_removed;
#endif
	}

	/*we need at least moov or meta*/
	if (!mov->moov && !mov->meta
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	        && !mov->moof && !mov->is_index_segment
#endif
	   ) {
		return GF_ISOM_INCOMPLETE_FILE;
	}
	/*we MUST have movie header*/
	if (!gf_opts_get_bool("core", "no-check")) {
		if (mov->moov && !mov->moov->mvhd) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing MVHD in MOOV!\n"));
			return GF_ISOM_INVALID_FILE;
		}

		/*we MUST have meta handler*/
		if (mov->meta && !mov->meta->handler) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] Missing handler in META!\n"));
			return GF_ISOM_INVALID_FILE;
		}
	}

#ifndef GPAC_DISABLE_ISOM_WRITE

	if (mov->moov) {
		/*set the default interleaving time*/
		mov->interleavingTime = mov->moov->mvhd->timeScale;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		/*in edit mode with successfully loaded fragments, delete all fragment signaling since
		file is no longer fragmented*/
		if ((mov->openMode > GF_ISOM_OPEN_READ) && (mov->openMode != GF_ISOM_OPEN_KEEP_FRAGMENTS) && mov->moov->mvex) {
			gf_isom_box_del_parent(&mov->moov->child_boxes, (GF_Box *)mov->moov->mvex);
			mov->moov->mvex = NULL;
		}
#endif

	}

	//create a default mdat if none was found
	if (!mov->mdat && (mov->openMode != GF_ISOM_OPEN_READ) && (mov->openMode != GF_ISOM_OPEN_KEEP_FRAGMENTS)) {
		mov->mdat = (GF_MediaDataBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MDAT);
		if (!mov->mdat) return GF_OUT_OF_MEM;
		e = gf_list_add(mov->TopBoxes, mov->mdat);
		if (e) return e;
	}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

	return GF_OK;
}

extern u64 unused_bytes;

GF_Err gf_isom_parse_movie_boxes(GF_ISOFile *mov, u32 *boxType, u64 *bytesMissing, Bool progressive_mode)
{
	GF_Err e;
	GF_Blob *blob = NULL;

	//if associated file is a blob, lock blob before parsing !
	if (mov->movieFileMap && ((mov->movieFileMap->type == GF_ISOM_DATA_MEM) || (mov->movieFileMap->type == GF_ISOM_DATA_FILE))) {
		blob = ((GF_FileDataMap *)mov->movieFileMap)->blob;
	}

	if (blob)
		gf_mx_p(blob->mx);

	unused_bytes = 0;
	e = gf_isom_parse_movie_boxes_internal(mov, boxType, bytesMissing, progressive_mode);

	if (blob)
		gf_mx_v(blob->mx);
	return e;

}

GF_ISOFile *gf_isom_new_movie()
{
	GF_ISOFile *mov = (GF_ISOFile*)gf_malloc(sizeof(GF_ISOFile));
	if (mov == NULL) {
		gf_isom_set_last_error(NULL, GF_OUT_OF_MEM);
		return NULL;
	}
	memset(mov, 0, sizeof(GF_ISOFile));

	/*init the boxes*/
	mov->TopBoxes = gf_list_new();
	if (!mov->TopBoxes) {
		gf_isom_set_last_error(NULL, GF_OUT_OF_MEM);
		gf_free(mov);
		return NULL;
	}

	/*default storage mode is flat*/
	mov->storageMode = GF_ISOM_STORE_FLAT;
	mov->es_id_default_sync = -1;
	return mov;
}

//Create and parse the movie for READ - EDIT only
GF_ISOFile *gf_isom_open_file(const char *fileName, GF_ISOOpenMode OpenMode, const char *tmp_dir)
{
	GF_Err e;
	u64 bytes;
	GF_ISOFile *mov = gf_isom_new_movie();
	if (!mov || !fileName) return NULL;

	mov->fileName = gf_strdup(fileName);
	mov->openMode = OpenMode;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (OpenMode==GF_ISOM_OPEN_READ_DUMP)
		mov->store_traf_map = GF_TRUE;
#endif

	if ( (OpenMode == GF_ISOM_OPEN_READ) || (OpenMode == GF_ISOM_OPEN_READ_DUMP) || (OpenMode == GF_ISOM_OPEN_READ_EDIT) ) {
		if (OpenMode == GF_ISOM_OPEN_READ_EDIT) {
			mov->openMode = GF_ISOM_OPEN_READ_EDIT;

			// create a memory edit map in case we add samples, typically during import
			e = gf_isom_datamap_new(NULL, tmp_dir, GF_ISOM_DATA_MAP_WRITE, & mov->editFileMap);
			if (e) {
				gf_isom_set_last_error(NULL, e);
				gf_isom_delete_movie(mov);
				return NULL;
			}
		} else {
			mov->openMode = GF_ISOM_OPEN_READ;
		}
		mov->es_id_default_sync = -1;
		//for open, we do it the regular way and let the GF_DataMap assign the appropriate struct
		//this can be FILE (the only one supported...) as well as remote
		//(HTTP, ...),not suported yet
		//the bitstream IS PART OF the GF_DataMap
		//as this is read-only, use a FileMapping. this is the only place where
		//we use file mapping
		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_READ_ONLY, &mov->movieFileMap);
		if (e) {
			gf_isom_set_last_error(NULL, e);
			gf_isom_delete_movie(mov);
			return NULL;
		}

		if (OpenMode == GF_ISOM_OPEN_READ_DUMP) {
			mov->FragmentsFlags |= GF_ISOM_FRAG_READ_DEBUG;
		}
	} else {

#ifdef GPAC_DISABLE_ISOM_WRITE
		//not allowed for READ_ONLY lib
		gf_isom_delete_movie(mov);
		gf_isom_set_last_error(NULL, GF_ISOM_INVALID_MODE);
		return NULL;

#else

		//set a default output name for edited file
		mov->finalName = (char*)gf_malloc(strlen(fileName) + 5);
		if (!mov->finalName) {
			gf_isom_set_last_error(NULL, GF_OUT_OF_MEM);
			gf_isom_delete_movie(mov);
			return NULL;
		}
		strcpy(mov->finalName, "out_");
		strcat(mov->finalName, fileName);

		//open the original file with edit tag
		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_EDIT, &mov->movieFileMap);
		//if the file doesn't exist, we assume it's wanted and create one from scratch
		if (e) {
			gf_isom_set_last_error(NULL, e);
			gf_isom_delete_movie(mov);
			return NULL;
		}
		//and create a temp fileName for the edit
		e = gf_isom_datamap_new("_gpac_isobmff_tmp_edit", tmp_dir, GF_ISOM_DATA_MAP_WRITE, & mov->editFileMap);
		if (e) {
			gf_isom_set_last_error(NULL, e);
			gf_isom_delete_movie(mov);
			return NULL;
		}

		mov->es_id_default_sync = -1;

#endif
	}

	//OK, let's parse the movie...
	mov->LastError = gf_isom_parse_movie_boxes(mov, NULL, &bytes, 0);

#if 0
	if (!mov->LastError && (OpenMode == GF_ISOM_OPEN_CAT_FRAGMENTS)) {
		gf_isom_datamap_del(mov->movieFileMap);
		/*reopen the movie file map in cat mode*/
		mov->LastError = gf_isom_datamap_new(fileName, tmp_dir, GF_ISOM_DATA_MAP_CAT, & mov->movieFileMap);
	}
#endif

	if (mov->LastError) {
		gf_isom_set_last_error(NULL, mov->LastError);
		gf_isom_delete_movie(mov);
		return NULL;
	}

	mov->nb_box_init_seg = gf_list_count(mov->TopBoxes);
	return mov;
}

GF_Err gf_isom_set_write_callback(GF_ISOFile *mov,
 			GF_Err (*on_block_out)(void *cbk, u8 *data, u32 block_size),
			GF_Err (*on_block_patch)(void *usr_data, u8 *block, u32 block_size, u64 block_offset, Bool is_insert),
 			void *usr_data,
 			u32 block_size)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	if (mov->finalName && !strcmp(mov->finalName, "_gpac_isobmff_redirect")) {}
	else if (mov->fileName && !strcmp(mov->fileName, "_gpac_isobmff_redirect")) {}
	else return GF_BAD_PARAM;
	mov->on_block_out = on_block_out;
	mov->on_block_patch = on_block_patch;
	mov->on_block_out_usr_data = usr_data;
	mov->on_block_out_block_size = block_size;
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}


u64 gf_isom_get_mp4time()
{
	u32 calctime, msec;
	u64 ret;
	gf_utc_time_since_1970(&calctime, &msec);
	calctime += GF_ISOM_MAC_TIME_OFFSET;
	ret = calctime;
	return ret;
}

void gf_isom_delete_movie(GF_ISOFile *mov)
{
	if (!mov) return;

	//these are our two main files
	if (mov->movieFileMap) gf_isom_datamap_del(mov->movieFileMap);

#ifndef GPAC_DISABLE_ISOM_WRITE
	if (mov->editFileMap) {
		gf_isom_datamap_del(mov->editFileMap);
	}
	if (mov->finalName) gf_free(mov->finalName);
#endif

	gf_isom_box_array_del(mov->TopBoxes);
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	gf_isom_box_array_del(mov->moof_list);
	if (mov->mfra)
		gf_isom_box_del((GF_Box*)mov->mfra);
	if (mov->sidx_pts_store)
		gf_free(mov->sidx_pts_store);
	if (mov->sidx_pts_next_store)
		gf_free(mov->sidx_pts_next_store);

	if (mov->main_sidx)
		gf_isom_box_del((GF_Box*)mov->main_sidx);

	if (mov->block_buffer)
		gf_free(mov->block_buffer);
#endif
	if (mov->last_producer_ref_time)
		gf_isom_box_del((GF_Box *) mov->last_producer_ref_time);
	if (mov->fileName) gf_free(mov->fileName);
	gf_free(mov);
}

GF_TrackBox *gf_isom_get_track_from_id(GF_MovieBox *moov, GF_ISOTrackID trackID)
{
	u32 i, count;
	if (!moov || !trackID) return NULL;

	count = gf_list_count(moov->trackList);
	for (i = 0; i<count; i++) {
		GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(moov->trackList, i);
		if (trak->Header->trackID == trackID) return trak;
	}
	return NULL;
}

GF_TrackBox *gf_isom_get_track_from_original_id(GF_MovieBox *moov, u32 originalID, u32 originalFile)
{
	u32 i, count;
	if (!moov || !originalID) return NULL;

	count = gf_list_count(moov->trackList);
	for (i = 0; i<count; i++) {
		GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(moov->trackList, i);
		if ((trak->originalFile == originalFile) && (trak->originalID == originalID)) return trak;
	}
	return NULL;
}

GF_TrackBox *gf_isom_get_track_from_file(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	if (!movie) return NULL;
	trak = gf_isom_get_track(movie->moov, trackNumber);
	if (!trak) movie->LastError = GF_BAD_PARAM;
	return trak;
}


//WARNING: MOVIETIME IS EXPRESSED IN MEDIA TS
GF_Err GetMediaTime(GF_TrackBox *trak, Bool force_non_empty, u64 movieTime, u64 *MediaTime, s64 *SegmentStartTime, s64 *MediaOffset, u8 *useEdit, u64 *next_edit_start_plus_one)
{
#if 0
	GF_Err e;
	u32 sampleNumber, prevSampleNumber;
	u64 firstDTS;
#endif
	u32 i, count;
	Bool last_is_empty = 0;
	u64 time, lastSampleTime;
	s64 mtime;
	GF_EdtsEntry *ent;
	Double scale_ts;
	GF_SampleTableBox *stbl = trak->Media->information->sampleTable;

	if (next_edit_start_plus_one) *next_edit_start_plus_one = 0;
	*useEdit = 1;
	*MediaTime = 0;
	//no segment yet...
	*SegmentStartTime = -1;
	*MediaOffset = -1;
	if (!trak->moov->mvhd->timeScale || !trak->Media->mediaHeader->timeScale || !stbl->SampleSize) {
		return GF_ISOM_INVALID_FILE;
	}

	//no samples...
	if (!stbl->SampleSize->sampleCount) {
		lastSampleTime = 0;
	} else {
		lastSampleTime = trak->Media->mediaHeader->duration;
	}

	//No edits, 1 to 1 mapping
	if (! trak->editBox || !trak->editBox->editList) {
		*MediaTime = movieTime;
		//check this is in our media time line
		if ((*MediaTime > lastSampleTime)
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		        && !trak->moov->mov->moof
#endif
		   ) {
			*MediaTime = lastSampleTime;
		}
		*useEdit = 0;
		return GF_OK;
	}
	//browse the edit list and get the time
	scale_ts = trak->Media->mediaHeader->timeScale;
	scale_ts /= trak->moov->mvhd->timeScale;

	time = 0;
	ent = NULL;
	count=gf_list_count(trak->editBox->editList->entryList);
	for (i=0; i<count; i++) {
		ent = (GF_EdtsEntry *)gf_list_get(trak->editBox->editList->entryList, i);
		if ( (time + ent->segmentDuration) * scale_ts > movieTime) {
			if (!force_non_empty || (ent->mediaTime >= 0)) {
				if (next_edit_start_plus_one) *next_edit_start_plus_one = 1 + (u64) ((time + ent->segmentDuration) * scale_ts);
				goto ent_found;
			}
		}
		time += ent->segmentDuration;
		last_is_empty = ent->segmentDuration ? 0 : 1;
	}

	if (last_is_empty) {
		ent = (GF_EdtsEntry *)gf_list_last(trak->editBox->editList->entryList);
		if (ent->mediaRate == 0x10000) {
			*MediaTime = movieTime + ent->mediaTime;
		} else {
			ent = (GF_EdtsEntry *)gf_list_get(trak->editBox->editList->entryList, 0);
			if (ent->mediaRate == -0x10000) {
				u64 dur = (u64) (ent->segmentDuration * scale_ts);
				*MediaTime = (movieTime > dur) ? (movieTime-dur) : 0;
			}
		}
		*useEdit = 0;
		return GF_OK;
	}


	//we had nothing in the list (strange file but compliant...)
	//return the 1 to 1 mapped vale of the last media sample
	if (!ent) {
		*MediaTime = movieTime;
		//check this is in our media time line
		if (*MediaTime > lastSampleTime) *MediaTime = lastSampleTime;
		*useEdit = 0;
		return GF_OK;
	}
	//request for a bigger time that what we can give: return the last sample (undefined behavior...)
	*MediaTime = lastSampleTime;
	return GF_OK;

ent_found:
	//OK, we found our entry, set the SegmentTime
	*SegmentStartTime = time;

	//we request an empty list, there's no media here...
	if (ent->mediaTime < 0) {
		*MediaTime = 0;
		return GF_OK;
	}
	//we request a dwell edit
	if (! ent->mediaRate) {
		*MediaTime = ent->mediaTime;
		//no media offset
		*MediaOffset = 0;
		*useEdit = 2;
		return GF_OK;
	}

	/*WARNING: this can be "-1" when doing searchForward mode (to prevent jumping to next entry)*/
	mtime = ent->mediaTime + movieTime - (time * trak->Media->mediaHeader->timeScale / trak->moov->mvhd->timeScale);
	if (mtime<0) mtime = 0;
	*MediaTime = (u64) mtime;
	*MediaOffset = ent->mediaTime;

#if 0
	//
	//Sanity check: is the requested time valid ? This is to cope with wrong EditLists
	//we have the translated time, but we need to make sure we have a sample at this time ...
	//we have to find a COMPOSITION time
	e = stbl_findEntryForTime(stbl, (u32) *MediaTime, 1, &sampleNumber, &prevSampleNumber);
	if (e) return e;

	//first case: our time is after the last sample DTS (it's a broken editList somehow)
	//set the media time to the last sample
	if (!sampleNumber && !prevSampleNumber) {
		*MediaTime = lastSampleTime;
		return GF_OK;
	}
	//get the appropriated sample
	if (!sampleNumber) sampleNumber = prevSampleNumber;

	stbl_GetSampleDTS(stbl->TimeToSample, sampleNumber, &DTS);
	CTS = 0;
	if (stbl->CompositionOffset) stbl_GetSampleCTS(stbl->CompositionOffset, sampleNumber, &CTS);

	//now get the entry sample (the entry time gives the CTS, and we need the DTS
	e = stbl_findEntryForTime(stbl, (u32) ent->mediaTime, 0, &sampleNumber, &prevSampleNumber);
	if (e) return e;

	//oops, the mediaTime indicates a sample that is not in our media !
	if (!sampleNumber && !prevSampleNumber) {
		*MediaTime = lastSampleTime;
		return GF_ISOM_INVALID_FILE;
	}
	if (!sampleNumber) sampleNumber = prevSampleNumber;

	stbl_GetSampleDTS(stbl->TimeToSample, sampleNumber, &firstDTS);

	//and store the "time offset" of the desired sample in this segment
	//this is weird, used to rebuild the timeStamp when reading from the track, not the
	//media ...
	*MediaOffset = firstDTS;
#endif
	return GF_OK;
}

GF_Err GetNextMediaTime(GF_TrackBox *trak, u64 movieTime, u64 *OutMovieTime)
{
	u32 i;
	u64 time;
	GF_EdtsEntry *ent;

	*OutMovieTime = 0;
	if (! trak->editBox || !trak->editBox->editList) return GF_BAD_PARAM;

	time = 0;
	ent = NULL;
	i=0;
	while ((ent = (GF_EdtsEntry *)gf_list_enum(trak->editBox->editList->entryList, &i))) {
		if (gf_timestamp_greater_or_equal(time, trak->moov->mvhd->timeScale, movieTime, trak->Media->mediaHeader->timeScale)) {
			/*skip empty edits*/
			if (ent->mediaTime >= 0) {
				*OutMovieTime = time * trak->Media->mediaHeader->timeScale / trak->moov->mvhd->timeScale;
				if (*OutMovieTime>0) *OutMovieTime -= 1;
				return GF_OK;
			}
		}
		time += ent->segmentDuration;
	}
	//request for a bigger time that what we can give: return the last sample (undefined behavior...)
	*OutMovieTime = trak->moov->mvhd->duration;
	return GF_EOS;
}

GF_Err GetPrevMediaTime(GF_TrackBox *trak, u64 movieTime, u64 *OutMovieTime)
{
	u32 i;
	u64 time;
	GF_EdtsEntry *ent;

	*OutMovieTime = 0;
	if (! trak->editBox || !trak->editBox->editList) return GF_BAD_PARAM;

	time = 0;
	ent = NULL;
	i=0;
	while ((ent = (GF_EdtsEntry *)gf_list_enum(trak->editBox->editList->entryList, &i))) {
		if (ent->mediaTime == -1) {
			if ( gf_timestamp_greater_or_equal(time + ent->segmentDuration, trak->moov->mvhd->timeScale, movieTime, trak->Media->mediaHeader->timeScale)) {
				*OutMovieTime = gf_timestamp_rescale(time, trak->moov->mvhd->timeScale, trak->Media->mediaHeader->timeScale);
				return GF_OK;
			}
			continue;
		}
		/*get the first entry whose end is greater than or equal to the desired time*/
		time += ent->segmentDuration;
		if (gf_timestamp_greater_or_equal(time, trak->moov->mvhd->timeScale, movieTime, trak->Media->mediaHeader->timeScale)) {
			*OutMovieTime = time * trak->Media->mediaHeader->timeScale / trak->moov->mvhd->timeScale;
			return GF_OK;
		}
	}
	*OutMovieTime = 0;
	return GF_OK;
}

#ifndef GPAC_DISABLE_ISOM_WRITE

GF_Err gf_isom_insert_moov(GF_ISOFile *file)
{
	GF_MovieHeaderBox *mvhd;
	if (file->moov) return GF_OK;

	//OK, create our boxes (mvhd, iods, ...)
	file->moov = (GF_MovieBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MOOV);
	if (!file->moov) return GF_OUT_OF_MEM;
	file->moov->mov = file;
	//Header SetUp
	mvhd = (GF_MovieHeaderBox *) gf_isom_box_new_parent(&file->moov->child_boxes, GF_ISOM_BOX_TYPE_MVHD);
	if (!mvhd) return GF_OUT_OF_MEM;

	if (gf_sys_is_test_mode() ) {
		mvhd->creationTime = mvhd->modificationTime = 0;
	} else {
		u64 now = gf_isom_get_mp4time();
		mvhd->creationTime = now;
		if (!file->keep_utc)
			mvhd->modificationTime = now;
	}

	mvhd->nextTrackID = 1;
	//600 is our default movie TimeScale
	mvhd->timeScale = 600;

	file->interleavingTime = mvhd->timeScale;
	moov_on_child_box((GF_Box*)file->moov, (GF_Box *)mvhd, GF_FALSE);
	gf_list_add(file->TopBoxes, file->moov);
	return GF_OK;
}

//Create the movie for WRITE only
GF_ISOFile *gf_isom_create_movie(const char *fileName, GF_ISOOpenMode OpenMode, const char *tmp_dir)
{
	GF_Err e;

	GF_ISOFile *mov = gf_isom_new_movie();
	if (!mov) return NULL;
	mov->openMode = OpenMode;
	//then set up our movie

	//in WRITE, the input dataMap is ALWAYS NULL
	mov->movieFileMap = NULL;

	//but we have the edit one
	if (OpenMode == GF_ISOM_OPEN_WRITE) {
		const char *ext;
		//THIS IS NOT A TEMP FILE, WRITE mode is used for "live capture"
		//this file will be the final file...
		mov->fileName = fileName ? gf_strdup(fileName) : NULL;
		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_WRITE, &mov->editFileMap);
		if (e) goto err_exit;

		/*brand is set to ISOM or QT by default - it may be touched until sample data is added to track*/
		ext = gf_file_ext_start(fileName);
		if (ext && (!strnicmp(ext, ".mov", 4) || !strnicmp(ext, ".qt", 3))) {
			gf_isom_set_brand_info((GF_ISOFile *) mov, GF_ISOM_BRAND_QT, 512);
		} else {
			gf_isom_set_brand_info((GF_ISOFile *) mov, GF_ISOM_BRAND_ISOM, 1);
		}
	} else {
		//we are in EDIT mode but we are creating the file -> temp file
		mov->finalName = fileName ? gf_strdup(fileName) : NULL;
		e = gf_isom_datamap_new("_gpac_isobmff_tmp_edit", tmp_dir, GF_ISOM_DATA_MAP_WRITE, &mov->editFileMap);
		if (e) {
			gf_isom_set_last_error(NULL, e);
			gf_isom_delete_movie(mov);
			return NULL;
		}
		//brand is set to ISOM by default
		gf_isom_set_brand_info( (GF_ISOFile *) mov, GF_ISOM_BRAND_ISOM, 1);
	}

	//create an MDAT
	mov->mdat = (GF_MediaDataBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MDAT);
	if (!mov->mdat) {
		gf_isom_set_last_error(NULL, GF_OUT_OF_MEM);
		gf_isom_delete_movie(mov);
		return NULL;
	}
	gf_list_add(mov->TopBoxes, mov->mdat);

	//default behavior is capture mode, no interleaving (eg, no rewrite of mdat)
	mov->storageMode = GF_ISOM_STORE_FLAT;
	return mov;

err_exit:
	gf_isom_set_last_error(NULL, e);
	if (mov) gf_isom_delete_movie(mov);
	return NULL;
}

GF_EdtsEntry *CreateEditEntry(u64 EditDuration, u64 MediaTime, u32 MediaRate, u8 EditMode)
{
	GF_EdtsEntry *ent;

	ent = (GF_EdtsEntry*)gf_malloc(sizeof(GF_EdtsEntry));
	if (!ent) return NULL;

	switch (EditMode) {
	case GF_ISOM_EDIT_EMPTY:
		ent->mediaRate = 0x10000;
		ent->mediaTime = -1;
		break;

	case GF_ISOM_EDIT_DWELL:
		ent->mediaRate = 0;
		ent->mediaTime = MediaTime;
		break;
	default:
		ent->mediaRate = MediaRate;
		ent->mediaTime = MediaTime;
		break;
	}
	ent->segmentDuration = EditDuration;
	return ent;
}

GF_Err gf_isom_add_subsample_info(GF_SubSampleInformationBox *sub_samples, u32 sampleNumber, u32 subSampleSize, u8 priority, u32 reserved, Bool discardable)
{
	u32 i, count, last_sample;
	GF_SubSampleInfoEntry *pSamp;
	GF_SubSampleEntry *pSubSamp;

	pSamp = NULL;
	last_sample = 0;
	count = gf_list_count(sub_samples->Samples);
	for (i=0; i<count; i++) {
		pSamp = (GF_SubSampleInfoEntry*) gf_list_get(sub_samples->Samples, i);
		/*TODO - do we need to support insertion of subsample info ?*/
		if (last_sample + pSamp->sample_delta > sampleNumber) return GF_NOT_SUPPORTED;
		if (last_sample + pSamp->sample_delta == sampleNumber) break;
		last_sample += pSamp->sample_delta;
		pSamp = NULL;
	}

	if (!pSamp) {
		GF_SAFEALLOC(pSamp, GF_SubSampleInfoEntry);
		if (!pSamp) return GF_OUT_OF_MEM;
		pSamp->SubSamples = gf_list_new();
		if (!pSamp->SubSamples ) {
			gf_free(pSamp);
			return GF_OUT_OF_MEM;
		}
		pSamp->sample_delta = sampleNumber - last_sample;
		gf_list_add(sub_samples->Samples, pSamp);
	}

	if ((subSampleSize>0xFFFF) && !sub_samples->version) {
		sub_samples->version = 1;
	}
	/*remove last subsample info*/
	if (!subSampleSize) {
		pSubSamp = gf_list_last(pSamp->SubSamples);
		gf_list_rem_last(pSamp->SubSamples);
		gf_free(pSubSamp);
		if (!gf_list_count(pSamp->SubSamples)) {
			gf_list_del_item(sub_samples->Samples, pSamp);
			gf_list_del(pSamp->SubSamples);
			gf_free(pSamp);
		}
		return GF_OK;
	}
	/*add subsample*/
	GF_SAFEALLOC(pSubSamp, GF_SubSampleEntry);
	if (!pSubSamp) return GF_OUT_OF_MEM;
	pSubSamp->subsample_size = subSampleSize;
	pSubSamp->subsample_priority = priority;
	pSubSamp->reserved = reserved;
	pSubSamp->discardable = discardable;
	return gf_list_add(pSamp->SubSamples, pSubSamp);
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#if 0 //unused
u32 gf_isom_sample_get_subsamples_count(GF_ISOFile *movie, u32 track)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, track);
	if (!track) return 0;
	if (!trak->Media || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->sub_samples) return 0;
	return gf_list_count(trak->Media->information->sampleTable->sub_samples);
}
#endif

Bool gf_isom_get_subsample_types(GF_ISOFile *movie, u32 track, u32 subs_index, u32 *flags)
{
	GF_SubSampleInformationBox *sub_samples=NULL;
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, track);

	if (!track || !subs_index) return GF_FALSE;
	if (!trak->Media || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->sub_samples) return GF_FALSE;
	sub_samples = gf_list_get(trak->Media->information->sampleTable->sub_samples, subs_index-1);
	if (!sub_samples) return GF_FALSE;
	*flags = sub_samples->flags;
	return GF_TRUE;
}

u32 gf_isom_sample_get_subsample_entry(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 flags, GF_SubSampleInfoEntry **sub_sample)
{
	u32 i, count, last_sample;
	GF_SubSampleInformationBox *sub_samples=NULL;
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, track);
	if (sub_sample) *sub_sample = NULL;
	if (!track) return 0;
	if (!trak->Media || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->sub_samples) return 0;
	count = gf_list_count(trak->Media->information->sampleTable->sub_samples);
	for (i=0; i<count; i++) {
		sub_samples = gf_list_get(trak->Media->information->sampleTable->sub_samples, i);
		if (sub_samples->flags==flags) break;
		sub_samples = NULL;
	}
	if (!sub_samples) return 0;

	last_sample = 0;
	count = gf_list_count(sub_samples->Samples);
	for (i=0; i<count; i++) {
		GF_SubSampleInfoEntry *pSamp = (GF_SubSampleInfoEntry *) gf_list_get(sub_samples->Samples, i);
		if (last_sample + pSamp->sample_delta == sampleNumber) {
			if (sub_sample) *sub_sample = pSamp;
			return gf_list_count(pSamp->SubSamples);
		}
		last_sample += pSamp->sample_delta;
	}
	return 0;
}
#endif /*GPAC_DISABLE_ISOM*/
