
/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2019
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

#if !defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)

#define GPAC_ISOM_CPRT_NOTICE "IsoMedia File Produced with GPAC"

#include <gpac/revision.h>
#define GPAC_ISOM_CPRT_NOTICE_VERSION GPAC_ISOM_CPRT_NOTICE" "GPAC_VERSION "-rev" GPAC_GIT_REVISION

static GF_Err gf_isom_insert_copyright(GF_ISOFile *movie)
{
	u32 i;
	GF_Box *a;
	GF_FreeSpaceBox *_free;
	i=0;
	while ((a = (GF_Box *)gf_list_enum(movie->TopBoxes, &i))) {
		if (a->type == GF_ISOM_BOX_TYPE_FREE) {
			_free = (GF_FreeSpaceBox *)a;
			if (_free->dataSize) {
				if (!strcmp(_free->data, GPAC_ISOM_CPRT_NOTICE_VERSION)) return GF_OK;
				if (strstr(_free->data, GPAC_ISOM_CPRT_NOTICE)) {
					gf_free(_free->data);
					_free->data = gf_strdup(gf_sys_is_test_mode() ? GPAC_ISOM_CPRT_NOTICE : GPAC_ISOM_CPRT_NOTICE_VERSION);
					_free->dataSize = 1 + (u32) strlen(_free->data);
					return GF_OK;
				}
			}
		}
	}
	a = gf_isom_box_new(GF_ISOM_BOX_TYPE_FREE);
	if (!a) return GF_OUT_OF_MEM;
	_free = (GF_FreeSpaceBox *)a;
	_free->data = gf_strdup(gf_sys_is_test_mode() ? GPAC_ISOM_CPRT_NOTICE : GPAC_ISOM_CPRT_NOTICE_VERSION);
	_free->dataSize = (u32) strlen(_free->data) + 1;
	if (!_free->data) return GF_OUT_OF_MEM;
	return gf_list_add(movie->TopBoxes, _free);
}

typedef struct
{
	/*the curent sample of this track*/
	u32 sampleNumber;
	/*timeScale of the media (for interleaving)*/
	u32 timeScale;
	/*this is for generic, time-based interleaving. Expressed in Media TimeScale*/
	u32 chunkDur;
	u32 chunkSize;
	u32 constant_size, constant_dur;

	u64 DTSprev;
	u8 isDone;
	u64 prev_offset;
	GF_MediaBox *mdia;
	GF_SampleTableBox *stbl;

	u32 all_dref_mode;

	/*each writer has a sampleToChunck and ChunkOffset tables
	these tables are filled during emulation mode and then will	replace the table in the GF_SampleTableBox*/
	GF_SampleToChunkBox *stsc;
	/*we don't know if it's a large offset or not*/
	GF_Box *stco;
	//track uses a box requiring seeking into the moov during write, we cannot dispatch blocks
	Bool prevent_dispatch;
} TrackWriter;

typedef struct
{
	char *buffer;
	u32 size;
	GF_ISOFile *movie;
	u32 total_samples, nb_done;
} MovieWriter;

void CleanWriters(GF_List *writers)
{
	TrackWriter *writer;
	while (gf_list_count(writers)) {
		writer = (TrackWriter*)gf_list_get(writers, 0);
		gf_isom_box_del(writer->stco);
		gf_isom_box_del((GF_Box *)writer->stsc);
		gf_free(writer);
		gf_list_rem(writers, 0);
	}
}

void ResetWriters(GF_List *writers)
{
	u32 i;
	TrackWriter *writer;
	i=0;
	while ((writer = (TrackWriter *)gf_list_enum(writers, &i))) {
		writer->isDone = 0;
		writer->chunkDur = 0;
		writer->chunkSize = 0;
		writer->DTSprev = 0;
		writer->sampleNumber = 1;
		gf_isom_box_del((GF_Box *)writer->stsc);
		writer->stsc = (GF_SampleToChunkBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSC);
		if (writer->stco->type == GF_ISOM_BOX_TYPE_STCO) {
			gf_free(((GF_ChunkOffsetBox *)writer->stco)->offsets);
			((GF_ChunkOffsetBox *)writer->stco)->offsets = NULL;
			((GF_ChunkOffsetBox *)writer->stco)->nb_entries = 0;
			((GF_ChunkOffsetBox *)writer->stco)->alloc_size = 0;
		} else {
			gf_free(((GF_ChunkLargeOffsetBox *)writer->stco)->offsets);
			((GF_ChunkLargeOffsetBox *)writer->stco)->offsets = NULL;
			((GF_ChunkLargeOffsetBox *)writer->stco)->nb_entries = 0;
			((GF_ChunkLargeOffsetBox *)writer->stco)->alloc_size = 0;
		}
	}
}

GF_Err SetupWriters(MovieWriter *mw, GF_List *writers, u8 interleaving)
{
	u32 i, trackCount;
	TrackWriter *writer;
	GF_TrackBox *trak;
	GF_ISOFile *movie = mw->movie;

	mw->total_samples = mw->nb_done = 0;
	if (!movie->moov) return GF_OK;

	trackCount = gf_list_count(movie->moov->trackList);
	for (i = 0; i < trackCount; i++) {
		trak = gf_isom_get_track(movie->moov, i+1);

		GF_SAFEALLOC(writer, TrackWriter);
		if (!writer) goto exit;
		writer->sampleNumber = 1;
		writer->mdia = trak->Media;
		writer->stbl = trak->Media->information->sampleTable;
		writer->timeScale = trak->Media->mediaHeader->timeScale;
		writer->all_dref_mode = Media_SelfContainedType(writer->mdia);

		if (trak->sample_encryption)
			writer->prevent_dispatch = GF_TRUE;

		writer->isDone = 0;
		writer->DTSprev = 0;
		writer->chunkDur = 0;
		writer->chunkSize = 0;
		writer->constant_size = writer->constant_dur = 0;
		if (writer->stbl->SampleSize->sampleSize)
			writer->constant_size = writer->stbl->SampleSize->sampleSize;
		if (writer->stbl->TimeToSample->nb_entries==1) {
			writer->constant_dur = writer->stbl->TimeToSample->entries[0].sampleDelta;
			if (writer->constant_dur>1) writer->constant_dur = 0;
		}
		if (!writer->constant_dur || !writer->constant_size || (writer->constant_size>=10))
			writer->constant_size = writer->constant_dur = 0;

		writer->stsc = (GF_SampleToChunkBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSC);
		if (writer->stbl->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
			writer->stco = gf_isom_box_new(GF_ISOM_BOX_TYPE_STCO);
		} else {
			writer->stco = gf_isom_box_new(GF_ISOM_BOX_TYPE_CO64);
		}
		/*stops from chunk escape*/
		if (interleaving) writer->stbl->MaxSamplePerChunk = 0;
		/*for progress, assume only one descIndex*/
		if (Media_IsSelfContained(writer->mdia, 1))
			mw->total_samples += writer->stbl->SampleSize->sampleCount;
		/*optimization for interleaving: put audio last (this can be overriden by priorities)*/
		if (movie->storageMode != GF_ISOM_STORE_INTERLEAVED) {
			gf_list_add(writers, writer);
		} else {
			if (writer->mdia->information->InfoHeader && writer->mdia->information->InfoHeader->type == GF_ISOM_BOX_TYPE_SMHD) {
				gf_list_add(writers, writer);
			} else {
				gf_list_insert(writers, writer, 0);
			}
		}
		if (movie->sample_groups_in_traf && trak->Media->information->sampleTable)
			trak->Media->information->sampleTable->skip_sample_groups = GF_TRUE;
	}
	return GF_OK;

exit:
	CleanWriters(writers);
	return GF_OUT_OF_MEM;
}


static void ShiftMetaOffset(GF_MetaBox *meta, u64 offset)
{
	u32 i, count;
	if (!meta->item_locations) return;

	count = gf_list_count(meta->item_locations->location_entries);
	for (i=0; i<count; i++) {
		GF_ItemLocationEntry *iloc = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, i);
		if (iloc->data_reference_index) continue;
		if (iloc->construction_method == 2) continue;
		if (!iloc->base_offset) {
			GF_ItemExtentEntry *entry = (GF_ItemExtentEntry *)gf_list_get(iloc->extent_entries, 0);
			if (entry && !entry->extent_length && !entry->original_extent_offset && (gf_list_count(iloc->extent_entries)==1) )
				continue;
		}

		iloc->base_offset += offset;
	}
}

static GF_Err ShiftOffset(GF_ISOFile *file, GF_List *writers, u64 offset)
{
	u32 i, j, k, l, last;
	TrackWriter *writer;
	GF_StscEntry *ent;

	if (file->meta) ShiftMetaOffset(file->meta, offset);
	if (file->moov && file->moov->meta) ShiftMetaOffset(file->moov->meta, offset);

	i=0;
	while ((writer = (TrackWriter *)gf_list_enum(writers, &i))) {
		if (writer->mdia->mediaTrack->meta) ShiftMetaOffset(writer->mdia->mediaTrack->meta, offset);

		//we have to proceed entry by entry in case a part of the media is not self-contained...
		for (j=0; j<writer->stsc->nb_entries; j++) {
			ent = &writer->stsc->entries[j];
			if ((writer->all_dref_mode==ISOM_DREF_EXT) || !Media_IsSelfContained(writer->mdia, ent->sampleDescriptionIndex))
				continue;

			//OK, get the chunk(s) number(s) and "shift" its (their) offset(s).
			if (writer->stco->type == GF_ISOM_BOX_TYPE_STCO) {
				GF_ChunkLargeOffsetBox *new_stco64 = NULL;
				GF_ChunkOffsetBox *stco = (GF_ChunkOffsetBox *) writer->stco;

				//be carefull for the last entry, nextChunk is set to 0 in edit mode...
				last = ent->nextChunk ? ent->nextChunk : stco->nb_entries + 1;
				for (k = ent->firstChunk; k < last; k++) {

					//we need to rewrite the table: only allocate co64 if not done previously and convert all offsets
					//to co64. Then (whether co64 was created or not) adjust the offset
					//Do not reassign table until we are done with the current sampleToChunk processing
					//since we have a test on stco->offsets[k-1], we need to keep stco untouched
					if (new_stco64 || file->force_co64 || (stco->offsets[k-1] + offset > 0xFFFFFFFF)) {
						if (!new_stco64) {
							new_stco64 = (GF_ChunkLargeOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CO64);
							if (!new_stco64) return GF_OUT_OF_MEM;
							new_stco64->nb_entries = stco->nb_entries;
							new_stco64->offsets = (u64 *) gf_malloc(new_stco64->nb_entries * sizeof(u64));
							if (!new_stco64->offsets) return GF_OUT_OF_MEM;
							//copy over the stco table
							for (l = 0; l < new_stco64->nb_entries; l++) {
								new_stco64->offsets[l] = (u64) stco->offsets[l];
							}
						}
						new_stco64->offsets[k-1] += offset;
					} else {
						stco->offsets[k-1] += (u32) offset;
					}
				}
				if (new_stco64) {
					//done with this sampleToChunk entry, replace the box if we moved to co64
					gf_isom_box_del(writer->stco);
					writer->stco = (GF_Box *)new_stco64;
					new_stco64 = NULL;
				}
			} else {
				GF_ChunkLargeOffsetBox *stco64 = (GF_ChunkLargeOffsetBox *) writer->stco;
				//be carefull for the last entry ...
				last = ent->nextChunk ? ent->nextChunk : stco64->nb_entries + 1;
				for (k = ent->firstChunk; k < last; k++) {
					stco64->offsets[k-1] += offset;
				}
			}
		}
	}

	return GF_OK;

}


//replace the chunk and offset tables...
static GF_Err WriteMoovAndMeta(GF_ISOFile *movie, GF_List *writers, GF_BitStream *bs)
{
	u32 i;
	TrackWriter *writer;
	GF_Err e;
	GF_Box *stco;
	GF_SampleToChunkBox *stsc;

	if (movie->meta) {
		//write the moov box...
		e = gf_isom_box_size((GF_Box *)movie->meta);
		if (e) return e;
		e = gf_isom_box_write((GF_Box *)movie->meta, bs);
		if (e) return e;
	}

	if (movie->moov) {
		Bool prevent_dispatch = GF_FALSE;
		//switch all our tables
		i=0;
		while ((writer = (TrackWriter*)gf_list_enum(writers, &i))) {
			//don't delete them !!!
			stsc = writer->stbl->SampleToChunk;
			stco = writer->stbl->ChunkOffset;
			s32 stsc_pos = gf_list_del_item(writer->stbl->child_boxes, stsc);
			s32 stco_pos = gf_list_del_item(writer->stbl->child_boxes, stco);
			writer->stbl->SampleToChunk = writer->stsc;
			writer->stbl->ChunkOffset = writer->stco;
			gf_list_insert(writer->stbl->child_boxes, writer->stsc, stsc_pos);
			gf_list_insert(writer->stbl->child_boxes, writer->stco, stco_pos);
			writer->stco = stco;
			writer->stsc = stsc;
			if (writer->prevent_dispatch)
				prevent_dispatch = GF_TRUE;
		}
		if (prevent_dispatch) {
			gf_bs_prevent_dispatch(bs, GF_TRUE);
		}
		//write the moov box...
		e = gf_isom_box_size((GF_Box *)movie->moov);
		if (e) return e;
		e = gf_isom_box_write((GF_Box *)movie->moov, bs);

		if (prevent_dispatch) {
			gf_bs_prevent_dispatch(bs, GF_FALSE);
		}

		//and re-switch our table. We have to do it that way because it is
		//needed when the moov is written first
		i=0;
		while ((writer = (TrackWriter*)gf_list_enum(writers, &i))) {
			//don't delete them !!!
			stsc = writer->stsc;
			stco = writer->stco;
			writer->stsc = writer->stbl->SampleToChunk;
			writer->stco = writer->stbl->ChunkOffset;
			s32 stsc_pos = gf_list_del_item(writer->stbl->child_boxes, writer->stsc);
			s32 stco_pos = gf_list_del_item(writer->stbl->child_boxes, writer->stco);

			writer->stbl->SampleToChunk = stsc;
			writer->stbl->ChunkOffset = stco;
			gf_list_insert(writer->stbl->child_boxes, stsc, stsc_pos);
			gf_list_insert(writer->stbl->child_boxes, stco, stco_pos);
		}
		if (e) return e;
	}
	return GF_OK;
}

//compute the size of the moov as it will be written.
u64 GetMoovAndMetaSize(GF_ISOFile *movie, GF_List *writers)
{
	u32 i;
	u64 size;
	TrackWriter *writer;

	size = 0;
	if (movie->moov) {
		gf_isom_box_size((GF_Box *)movie->moov);
		size = movie->moov->size;
		if (size > 0xFFFFFFFF) size += 8;

		i=0;
		while ((writer = (TrackWriter*)gf_list_enum(writers, &i))) {
			size -= writer->stbl->ChunkOffset->size;
			size -= writer->stbl->SampleToChunk->size;
			gf_isom_box_size((GF_Box *)writer->stsc);
			gf_isom_box_size(writer->stco);
			size += writer->stsc->size;
			size += writer->stco->size;
		}
	}
	if (movie->meta) {
		u64 msize;
		gf_isom_box_size((GF_Box *)movie->meta);
		msize = movie->meta->size;
		if (msize > 0xFFFFFFFF) msize += 8;
		size += msize;
	}
	return size;
}

static void muxer_report_progress(MovieWriter *mw)
{
	if (mw->movie->progress_cbk) {
		mw->movie->progress_cbk(mw->movie->progress_cbk_udta, mw->nb_done, mw->total_samples);
	} else {
		gf_set_progress("ISO File Writing", mw->nb_done, mw->total_samples);
	}
}

//Write a sample to the file - this is only called for self-contained media
GF_Err WriteSample(MovieWriter *mw, u32 size, u64 offset, u8 isEdited, GF_BitStream *bs, u32 nb_samp)
{
	GF_DataMap *map;
	u32 bytes;

	if (!size) return GF_OK;

	if (size>mw->size) {
		mw->buffer = (char*)gf_realloc(mw->buffer, size);
		mw->size = size;
	}

	if (!mw->buffer) return GF_OUT_OF_MEM;

	if (isEdited) {
		map = mw->movie->editFileMap;
	} else {
		map = mw->movie->movieFileMap;
	}
	//get the payload...
	bytes = gf_isom_datamap_get_data(map, mw->buffer, size, offset);
	if (bytes != size)
		return GF_IO_ERR;
	//write it to our stream...
	bytes = gf_bs_write_data(bs, mw->buffer, size);
	if (bytes != size)
		return GF_IO_ERR;

	mw->nb_done+=nb_samp;
	muxer_report_progress(mw);
	return GF_OK;
}

//flush as much as possible from current chunk for constand size and duration (typically raw audio)
// We don't want to write samples outside of the current source chunk
//since the next chunk might be edited (different bitstream object), which would complexify WriteSample code
//not flushing the chunk will work, but result in very slow writing of raw audio
void update_writer_constant_dur(GF_ISOFile *movie, TrackWriter *tkw, GF_StscEntry *stsc_ent, u32 *nb_samp, u32 *samp_size, Bool is_flat)
{
	u64 chunk_dur;
	u32 nb_in_run;
	u32 samp_idx_in_chunk, nb_samp_left_in_src_chunk;
	if (!tkw->constant_dur) return;

	samp_idx_in_chunk = tkw->sampleNumber - tkw->stbl->SampleToChunk->firstSampleInCurrentChunk;
	nb_samp_left_in_src_chunk = stsc_ent->samplesPerChunk - samp_idx_in_chunk;

	if (nb_samp_left_in_src_chunk<=1) return;

	if (is_flat) {
		nb_in_run = nb_samp_left_in_src_chunk;
	} else {

		chunk_dur =  movie->interleavingTime * tkw->timeScale;
		chunk_dur /= movie->moov->mvhd->timeScale;
		chunk_dur -= tkw->chunkDur;

		if (chunk_dur <= tkw->chunkDur) return;

		chunk_dur -= tkw->constant_dur;

		nb_in_run = (u32) (chunk_dur / tkw->constant_dur);

		if (nb_in_run > nb_samp_left_in_src_chunk) {
			nb_in_run = nb_samp_left_in_src_chunk;
		}
	}
	if (tkw->sampleNumber + nb_in_run >= tkw->stbl->SampleSize->sampleCount) {
		nb_in_run = tkw->stbl->SampleSize->sampleCount - tkw->sampleNumber;
	}

	chunk_dur = nb_in_run * tkw->constant_dur;

	tkw->chunkDur += (u32) chunk_dur;
	tkw->DTSprev += chunk_dur - tkw->constant_dur; //because nb_samp += nb_in_run-1

	*nb_samp = nb_in_run;
	*samp_size = nb_in_run * tkw->constant_size;
}

GF_Err DoWriteMeta(GF_ISOFile *file, GF_MetaBox *meta, GF_BitStream *bs, Bool Emulation, u64 baseOffset, u64 *mdatSize)
{
	GF_ItemExtentEntry *entry;
	u64 maxExtendOffset, maxExtendSize;
	u32 i, j, count;

	maxExtendOffset = 0;
	maxExtendSize = 0;
	*mdatSize = 0;
	if (!meta->item_locations) return GF_OK;

	count = gf_list_count(meta->item_locations->location_entries);
	for (i=0; i<count; i++) {
		u64 it_size;
		GF_ItemLocationEntry *iloc = (GF_ItemLocationEntry *)gf_list_get(meta->item_locations->location_entries, i);
		/*get item info*/
		GF_ItemInfoEntryBox *iinf = NULL;
		j=0;
		while ((iinf = (GF_ItemInfoEntryBox *)gf_list_enum(meta->item_infos->item_infos, &j))) {
			if (iinf->item_ID==iloc->item_ID) break;
			iinf = NULL;
		}

		if (!iloc->base_offset && (gf_list_count(iloc->extent_entries)==1)) {
			entry = (GF_ItemExtentEntry *)gf_list_get(iloc->extent_entries, 0);
			if (!entry->extent_length && !entry->original_extent_offset && !entry->extent_index) {
				entry->extent_offset = 0;
				continue;
			}
		}

		it_size = 0;
		/*for self contained only*/
		if (!iloc->data_reference_index) {
			if (iloc->construction_method != 2) {
				iloc->base_offset = baseOffset;
			}

			/*new resource*/
			if (iinf && iinf->full_path) {
				FILE *src=NULL;

				if (!iinf->data_len) {
					src = gf_fopen(iinf->full_path, "rb");
					if (!src) continue;
					gf_fseek(src, 0, SEEK_END);
					it_size = gf_ftell(src);
					gf_fseek(src, 0, SEEK_SET);
				} else {
					it_size = iinf->data_len;
				}
				if (maxExtendSize<it_size) maxExtendSize = it_size;

				if (!gf_list_count(iloc->extent_entries)) {
					GF_SAFEALLOC(entry, GF_ItemExtentEntry);
					gf_list_add(iloc->extent_entries, entry);
				}
				entry = (GF_ItemExtentEntry *)gf_list_get(iloc->extent_entries, 0);
				entry->extent_offset = 0;
				entry->extent_length = it_size;

				/*OK write to mdat*/
				if (!Emulation) {
					if (src) {
						char cache_data[4096];
						u64 remain = entry->extent_length;
						while (remain) {
							u32 size_cache = (remain>4096) ? 4096 : (u32) remain;
							size_t read = fread(cache_data, sizeof(char), size_cache, src);
							if (read ==(size_t) -1) break;
							gf_bs_write_data(bs, cache_data, (u32) read);
							remain -= (u32) read;
						}
					} else {
						gf_bs_write_data(bs, iinf->full_path, iinf->data_len);
					}
				}
				if (src) gf_fclose(src);
			}
			else if (gf_list_count(iloc->extent_entries)) {
				u32 j;
				j=0;
				while ((entry = (GF_ItemExtentEntry *)gf_list_enum(iloc->extent_entries, &j))) {
					if (entry->extent_index) continue;
					if (j && (maxExtendOffset<it_size) ) maxExtendOffset = it_size;
					/*compute new offset*/
					entry->extent_offset = baseOffset + it_size;

					it_size += entry->extent_length;
					if (maxExtendSize<entry->extent_length) maxExtendSize = entry->extent_length;

					/*Reading from the input file*/
					if (!Emulation) {
						char cache_data[4096];
						u64 remain = entry->extent_length;
						gf_bs_seek(file->movieFileMap->bs, entry->original_extent_offset + iloc->original_base_offset);
						while (remain) {
							u32 size_cache = (remain>4096) ? 4096 : (u32) remain;
							gf_bs_read_data(file->movieFileMap->bs, cache_data, size_cache);
							/*Writing to the output file*/
							gf_bs_write_data(bs, cache_data, size_cache);
							remain -= size_cache;
						}
					}
				}
			}
			baseOffset += it_size;
			*mdatSize += it_size;
		} else {
			/*we MUST have at least one extent for the dref data*/
			if (!gf_list_count(iloc->extent_entries)) {
				GF_SAFEALLOC(entry, GF_ItemExtentEntry);
				gf_list_add(iloc->extent_entries, entry);
			}
			entry = (GF_ItemExtentEntry *)gf_list_get(iloc->extent_entries, 0);
			entry->extent_offset = 0;
			/*0 means full length of referenced file*/
			entry->extent_length = 0;
		}
	}
	/*update offset & size length fields*/
	if (baseOffset>0xFFFFFFFF) meta->item_locations->base_offset_size = 8;
	else if (baseOffset) meta->item_locations->base_offset_size = 4;

	if (maxExtendSize>0xFFFFFFFF) meta->item_locations->length_size = 8;
	else if (maxExtendSize) meta->item_locations->length_size = 4;

	if (maxExtendOffset>0xFFFFFFFF) meta->item_locations->offset_size = 8;
	else if (maxExtendOffset) meta->item_locations->offset_size = 4;
	return GF_OK;
}

//this function writes track by track in the order of tracks inside the moov...
GF_Err DoWrite(MovieWriter *mw, GF_List *writers, GF_BitStream *bs, u8 Emulation, u64 StartOffset)
{
	u32 i;
	GF_Err e;
	TrackWriter *writer;
	u64 offset, sampOffset, predOffset;
	u32 chunkNumber, descIndex, sampSize;
	Bool force;
	GF_StscEntry *stsc_ent;
	u64 size, mdatSize = 0;
	GF_ISOFile *movie = mw->movie;

	/*write meta content first - WE DON'T support fragmentation of resources in ISOM atm*/
	if (movie->openMode != GF_ISOM_OPEN_WRITE) {
		if (movie->meta) {
			e = DoWriteMeta(movie, movie->meta, bs, Emulation, StartOffset, &size);
			if (e) return e;
			mdatSize += size;
			StartOffset += size;
		}
		if (movie->moov && movie->moov->meta) {
			e = DoWriteMeta(movie, movie->meta, bs, Emulation, StartOffset, &size);
			if (e) return e;
			mdatSize += size;
			StartOffset += size;
		}
		i=0;
		while ((writer = (TrackWriter*)gf_list_enum(writers, &i))) {
			if (writer->mdia->mediaTrack->meta) {
				e = DoWriteMeta(movie, movie->meta, bs, Emulation, StartOffset, &size);
				if (e) return e;
				mdatSize += size;
				StartOffset += size;
			}
		}
	}

	offset = StartOffset;
	predOffset = 0;
	i=0;
	while ((writer = (TrackWriter*)gf_list_enum(writers, &i))) {
		while (!writer->isDone) {
			Bool self_contained;
			u32 nb_samp=1;
			//To Check: are empty sample tables allowed ???
			if (writer->sampleNumber > writer->stbl->SampleSize->sampleCount) {
				writer->isDone = 1;
				continue;
			}
			e = stbl_GetSampleInfos(writer->stbl, writer->sampleNumber, &sampOffset, &chunkNumber, &descIndex, &stsc_ent);
			if (e) return e;
			e = stbl_GetSampleSize(writer->stbl->SampleSize, writer->sampleNumber, &sampSize);
			if (e) return e;

			update_writer_constant_dur(movie, writer, stsc_ent, &nb_samp, &sampSize, GF_TRUE);

			//update our chunks.
			force = 0;
			if (movie->openMode == GF_ISOM_OPEN_WRITE) {
				offset = sampOffset;
				if (predOffset != offset)
					force = 1;
			}

			if (writer->stbl->MaxChunkSize && (writer->chunkSize + sampSize > writer->stbl->MaxChunkSize)) {
				writer->chunkSize = 0;
				force = 1;
			}
			writer->chunkSize += sampSize;

			self_contained = ((writer->all_dref_mode==ISOM_DREF_SELF) || Media_IsSelfContained(writer->mdia, descIndex) ) ? GF_TRUE : GF_FALSE;

			//update our global offset...
			if (self_contained) {
				e = stbl_SetChunkAndOffset(writer->stbl, writer->sampleNumber, descIndex, writer->stsc, &writer->stco, offset, force, nb_samp);
				if (e) return e;
				if (movie->openMode == GF_ISOM_OPEN_WRITE) {
					predOffset = sampOffset + sampSize;
				} else {
					offset += sampSize;
					mdatSize += sampSize;
				}
			} else {
				if (predOffset != offset) force = 1;
				predOffset = sampOffset + sampSize;
				//we have a DataRef, so use the offset idicated in sampleToChunk and ChunkOffset tables...
				e = stbl_SetChunkAndOffset(writer->stbl, writer->sampleNumber, descIndex, writer->stsc, &writer->stco, sampOffset, force, nb_samp);
				if (e) return e;
			}
			//we write the sample if not emulation
			if (!Emulation) {
				if (self_contained) {
					e = WriteSample(mw, sampSize, sampOffset, stsc_ent->isEdited, bs, 1);
					if (e) return e;
				}
			}
			//ok, the track is done
			if (writer->sampleNumber >= writer->stbl->SampleSize->sampleCount) {
				writer->isDone = 1;
			} else {
				writer->sampleNumber += nb_samp;
			}
		}
	}
	//set the mdatSize...
	movie->mdat->dataSize = mdatSize;
	return GF_OK;
}


//write the file track by track, with moov box before or after the mdat
static GF_Err WriteFlat(MovieWriter *mw, u8 moovFirst, GF_BitStream *bs, Bool non_seakable, Bool for_fragments)
{
	GF_Err e;
	u32 i;
	u64 offset, finalOffset, totSize, begin, firstSize, finalSize;
	GF_Box *a, *cprt_box=NULL;
	GF_List *writers = gf_list_new();
	GF_ISOFile *movie = mw->movie;
	s32 moov_meta_pos=-1;
	begin = totSize = 0;

	//first setup the writers
	e = SetupWriters(mw, writers, 0);
	if (e) goto exit;

	if (!moovFirst) {
		if ((movie->openMode == GF_ISOM_OPEN_WRITE) && !non_seakable) {
			begin = 0;
			totSize = gf_isom_datamap_get_offset(movie->editFileMap);
			/*start boxes have not been written yet, do it*/
			if (!totSize) {
				if (movie->is_jp2) {
					gf_bs_write_u32(movie->editFileMap->bs, 12);
					gf_bs_write_u32(movie->editFileMap->bs, GF_ISOM_BOX_TYPE_JP);
					gf_bs_write_u32(movie->editFileMap->bs, 0x0D0A870A);
					totSize += 12;
					begin += 12;
				}
				if (movie->brand) {
					e = gf_isom_box_size((GF_Box *)movie->brand);
					if (e) goto exit;
					e = gf_isom_box_write((GF_Box *)movie->brand, movie->editFileMap->bs);
					if (e) goto exit;
					totSize += movie->brand->size;
					begin += movie->brand->size;
				}
				if (movie->pdin) {
					e = gf_isom_box_size((GF_Box *)movie->pdin);
					if (e) goto exit;
					e = gf_isom_box_write((GF_Box *)movie->pdin, movie->editFileMap->bs);
					if (e) goto exit;
					totSize += movie->pdin->size;
					begin += movie->pdin->size;
				}
			} else {
				if (movie->is_jp2) begin += 12;
				if (movie->brand) begin += movie->brand->size;
				if (movie->pdin) begin += movie->pdin->size;
			}
			totSize -= begin;
		} else if (!non_seakable || for_fragments) {
			if (movie->is_jp2) {
				gf_bs_write_u32(bs, 12);
				gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_JP);
				gf_bs_write_u32(bs, 0x0D0A870A);
			}
			if (movie->brand) {
				e = gf_isom_box_size((GF_Box *)movie->brand);
				if (e) goto exit;
				e = gf_isom_box_write((GF_Box *)movie->brand, bs);
				if (e) goto exit;
			}
			/*then progressive download*/
			if (movie->pdin) {
				e = gf_isom_box_size((GF_Box *)movie->pdin);
				if (e) goto exit;
				e = gf_isom_box_write((GF_Box *)movie->pdin, bs);
				if (e) goto exit;
			}
		}

		//if the moov is at the end, write directly
		i=0;
		while ((a = (GF_Box*)gf_list_enum(movie->TopBoxes, &i))) {
			switch (a->type) {
			/*written by hand*/
			case GF_ISOM_BOX_TYPE_MOOV:
			case GF_ISOM_BOX_TYPE_META:
				moov_meta_pos = i-1;
			case GF_ISOM_BOX_TYPE_FTYP:
			case GF_ISOM_BOX_TYPE_PDIN:
#ifndef GPAC_DISABLE_ISOM_ADOBE
			case GF_ISOM_BOX_TYPE_AFRA:
			case GF_ISOM_BOX_TYPE_ABST:
#endif
				break;
			case GF_ISOM_BOX_TYPE_MDAT:
				//in case we're capturing
				if (movie->openMode == GF_ISOM_OPEN_WRITE) {
					//emulate a write to recreate our tables (media data already written)
					e = DoWrite(mw, writers, bs, 1, begin);
					if (e) goto exit;
					continue;
				}
				if (non_seakable) {
					begin = gf_bs_get_position(bs);
					//do a sim pass to get the true mdat size
					e = DoWrite(mw, writers, bs, 1, begin);
					if (e) goto exit;

					if (movie->mdat->dataSize > 0xFFFFFFFF) {
						gf_bs_write_u32(bs, 1);
					} else {
						gf_bs_write_u32(bs, (u32) movie->mdat->dataSize + 8);
					}
					gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_MDAT);
					if (movie->mdat->dataSize > 0xFFFFFFFF) gf_bs_write_u64(bs, movie->mdat->dataSize + 8 + 8);
					//reset writers and write samples
					ResetWriters(writers);
					e = DoWrite(mw, writers, bs, 0, gf_bs_get_position(bs));
					if (e) goto exit;
					movie->mdat->size = movie->mdat->dataSize;
					totSize = 0;
				} else {
					//to avoid computing the size each time write always 4 + 4 + 8 bytes before
					begin = gf_bs_get_position(bs);
					gf_bs_write_u64(bs, 0);
					gf_bs_write_u64(bs, 0);
					e = DoWrite(mw, writers, bs, 0, gf_bs_get_position(bs));
					if (e) goto exit;
					totSize = gf_bs_get_position(bs) - begin;
				}
				break;

			case GF_ISOM_BOX_TYPE_FREE:
				//for backward compat with old arch, keep copyright before moov
				if (((GF_FreeSpaceBox*)a)->dataSize>4) {
					GF_FreeSpaceBox *fr = (GF_FreeSpaceBox*) a;
					if ((fr->dataSize>20) && !strncmp(fr->data, "IsoMedia File", 13)) {
						e = gf_isom_box_size(a);
						if (e) goto exit;
						e = gf_isom_box_write(a, bs);
						if (e) goto exit;
						cprt_box = a;
						break;
					}
				}
			default:
				if (moov_meta_pos < 0) {
					e = gf_isom_box_size(a);
					if (e) goto exit;
					e = gf_isom_box_write(a, bs);
					if (e) goto exit;
				}
				break;
			}
		}

		//OK, write the movie box.
		e = WriteMoovAndMeta(movie, writers, bs);
		if (e) goto exit;

#ifndef GPAC_DISABLE_ISOM_ADOBE
		i=0;
		while ((a = (GF_Box*)gf_list_enum(movie->TopBoxes, &i))) {
			switch (a->type) {
			case GF_ISOM_BOX_TYPE_AFRA:
			case GF_ISOM_BOX_TYPE_ABST:
				e = gf_isom_box_size(a);
				if (e) goto exit;
				e = gf_isom_box_write(a, bs);
				if (e) goto exit;
				break;
			}
		}
#endif

		/*if data has been written, update mdat size*/
		if (totSize) {
			offset = gf_bs_get_position(bs);
			e = gf_bs_seek(bs, begin);
			if (e) goto exit;
			if (totSize > 0xFFFFFFFF) {
				gf_bs_write_u32(bs, 1);
			} else {
				gf_bs_write_u32(bs, (u32) totSize);
			}
			gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_MDAT);
			if (totSize > 0xFFFFFFFF) gf_bs_write_u64(bs, totSize);
			e = gf_bs_seek(bs, offset);
			movie->mdat->size = totSize;
		}

		//then the rest
		i = (u32) (moov_meta_pos + 1);
		while ((a = (GF_Box*)gf_list_enum(movie->TopBoxes, &i))) {
			if (a==cprt_box) continue;

			switch (a->type) {
			case GF_ISOM_BOX_TYPE_MOOV:
			case GF_ISOM_BOX_TYPE_META:
			case GF_ISOM_BOX_TYPE_FTYP:
			case GF_ISOM_BOX_TYPE_PDIN:
			case GF_ISOM_BOX_TYPE_MDAT:
				break;
			default:
				e = gf_isom_box_size(a);
				if (e) goto exit;
				e = gf_isom_box_write(a, bs);
				if (e) goto exit;
			}
		}
		goto exit;
	}

	//nope, we have to write the moov first. The pb is that
	//1 - we don't know its size till the mdat is written
	//2 - we don't know the ofset at which the mdat will start...
	//3 - once the mdat is written, the chunkOffset table can have changed...

	if (movie->is_jp2) {
		gf_bs_write_u32(bs, 12);
		gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_JP);
		gf_bs_write_u32(bs, 0x0D0A870A);
	}
	if (movie->brand) {
		e = gf_isom_box_size((GF_Box *)movie->brand);
		if (e) goto exit;
		e = gf_isom_box_write((GF_Box *)movie->brand, bs);
		if (e) goto exit;
	}
	/*then progressive dnload*/
	if (movie->pdin) {
		e = gf_isom_box_size((GF_Box *)movie->pdin);
		if (e) goto exit;
		e = gf_isom_box_write((GF_Box *)movie->pdin, bs);
		if (e) goto exit;
	}

	//write all boxes before moov
	i=0;
	while ((a = (GF_Box*)gf_list_enum(movie->TopBoxes, &i))) {
		switch (a->type) {
		case GF_ISOM_BOX_TYPE_MOOV:
		case GF_ISOM_BOX_TYPE_META:
			moov_meta_pos = i-1;
			break;
		case GF_ISOM_BOX_TYPE_FTYP:
		case GF_ISOM_BOX_TYPE_PDIN:
		case GF_ISOM_BOX_TYPE_MDAT:
			break;
		//for backward compat with old arch keep out copyright after moov
		case GF_ISOM_BOX_TYPE_FREE:
			if (((GF_FreeSpaceBox*)a)->dataSize>4) {
				GF_FreeSpaceBox *fr = (GF_FreeSpaceBox*) a;
				if ((fr->dataSize>20) && !strncmp(fr->data, "IsoMedia File", 13)) {
					cprt_box = a;
					break;
				}
			}
		default:
			if (moov_meta_pos<0) {
				e = gf_isom_box_size(a);
				if (e) goto exit;
				e = gf_isom_box_write(a, bs);
				if (e) goto exit;
			}
			break;
		}
	}

	//What we will do is first emulate the write from the beginning...
	//note: this will set the size of the mdat
	e = DoWrite(mw, writers, bs, 1, gf_bs_get_position(bs));
	if (e) goto exit;

	firstSize = GetMoovAndMetaSize(movie, writers);
	//offset = (firstSize > 0xFFFFFFFF ? firstSize + 8 : firstSize) + 8 + (movie->mdat->dataSize > 0xFFFFFFFF ? 8 : 0);
	offset = firstSize + 8 + (movie->mdat->dataSize > 0xFFFFFFFF ? 8 : 0);
	e = ShiftOffset(movie, writers, offset);
	if (e) goto exit;
	//get the size and see if it has changed (eg, we moved to 64 bit offsets)
	finalSize = GetMoovAndMetaSize(movie, writers);
	if (firstSize != finalSize) {
		finalOffset = finalSize + 8 + (movie->mdat->dataSize > 0xFFFFFFFF ? 8 : 0);
		//OK, now we're sure about the final size.
		//we don't need to re-emulate, as the only thing that changed is the offset
		//so just shift the offset
		e = ShiftOffset(movie, writers, finalOffset - offset);
		if (e) goto exit;
	}
	//now write our stuff
	e = WriteMoovAndMeta(movie, writers, bs);
	if (e) goto exit;
	e = gf_isom_box_size((GF_Box *)movie->mdat);
	if (e) goto exit;
	e = gf_isom_box_write((GF_Box *)movie->mdat, bs);
	if (e) goto exit;

	//we don't need the offset as the moov is already written...
	ResetWriters(writers);
	e = DoWrite(mw, writers, bs, 0, 0);
	if (e) goto exit;

	//then the rest
	i=0;
	while ((a = (GF_Box*)gf_list_enum(movie->TopBoxes, &i))) {
		if ((i-1<= (u32) moov_meta_pos) && (a!=cprt_box)) continue;
		switch (a->type) {
		case GF_ISOM_BOX_TYPE_MOOV:
		case GF_ISOM_BOX_TYPE_META:
		case GF_ISOM_BOX_TYPE_FTYP:
		case GF_ISOM_BOX_TYPE_PDIN:
		case GF_ISOM_BOX_TYPE_MDAT:
			break;
		default:
			e = gf_isom_box_size(a);
			if (e) goto exit;
			e = gf_isom_box_write(a, bs);
			if (e) goto exit;
		}
	}

exit:
	CleanWriters(writers);
	gf_list_del(writers);
	return e;
}

GF_Err DoFullInterleave(MovieWriter *mw, GF_List *writers, GF_BitStream *bs, u8 Emulation, u64 StartOffset)
{

	u32 i, tracksDone;
	TrackWriter *tmp, *curWriter, *prevWriter;
	GF_Err e;
	u64 DTS, DTStmp, TStmp;
	s64 res;
	u32 descIndex, sampSize, chunkNumber;
	u16 curGroupID, curTrackPriority;
	Bool forceNewChunk, writeGroup;
	GF_StscEntry *stsc_ent;
	//this is used to emulate the write ...
	u64 offset, totSize, sampOffset;
	GF_ISOFile *movie = mw->movie;

	totSize = 0;
	curGroupID = 1;

	prevWriter = NULL;
	//we emulate a write from this offset...
	offset = StartOffset;
	tracksDone = 0;

	//browse each groups
	while (1) {
		writeGroup = 1;

		//proceed a group
		while (writeGroup) {
			u32 nb_samp = 1;
			Bool self_contained, chunked_forced=GF_FALSE;
			//first get the appropriated sample for the min time in this group
			curWriter = NULL;
			DTStmp = (u64) -1;
			TStmp = 0;
			curTrackPriority = (u16) -1;

			i=0;
			while ((tmp = (TrackWriter*)gf_list_enum(writers, &i))) {

				//is it done writing ?
				//is it in our group ??
				if (tmp->isDone || tmp->stbl->groupID != curGroupID) continue;

				//OK, get the current sample in this track
				stbl_GetSampleDTS(tmp->stbl->TimeToSample, tmp->sampleNumber, &DTS);
				res = TStmp ? DTStmp * tmp->timeScale - DTS * TStmp : 0;
				if (res < 0) continue;
				if ((!res) && curTrackPriority <= tmp->stbl->trackPriority) continue;
				curWriter = tmp;
				curTrackPriority = tmp->stbl->trackPriority;
				DTStmp = DTS;
				TStmp = tmp->timeScale;
			}
			//no sample found, we're done with this group
			if (!curWriter) {
				//we're done with the group
				writeGroup = 0;
				continue;
			}
			//To Check: are empty sample tables allowed ???
			if (curWriter->sampleNumber > curWriter->stbl->SampleSize->sampleCount) {
				curWriter->isDone = 1;
				tracksDone ++;
				continue;
			}

			e = stbl_GetSampleInfos(curWriter->stbl, curWriter->sampleNumber, &sampOffset, &chunkNumber, &descIndex, &stsc_ent);
			if (e) return e;
			e = stbl_GetSampleSize(curWriter->stbl->SampleSize, curWriter->sampleNumber, &sampSize);
			if (e) return e;

			update_writer_constant_dur(movie, curWriter, stsc_ent, &nb_samp, &sampSize, GF_FALSE);

			if (curWriter->stbl->MaxChunkSize && (curWriter->chunkSize + sampSize > curWriter->stbl->MaxChunkSize)) {
				curWriter->chunkSize = 0;
				chunked_forced = forceNewChunk = 1;
			}
			curWriter->chunkSize += sampSize;

			self_contained = ((curWriter->all_dref_mode==ISOM_DREF_SELF) || Media_IsSelfContained(curWriter->mdia, descIndex) ) ? GF_TRUE : GF_FALSE;

			//do we actually write, or do we emulate ?
			if (Emulation) {
				//are we in the same track ??? If not, force a new chunk when adding this sample
				if (!chunked_forced) {
					if (curWriter != prevWriter) {
						forceNewChunk = 1;
					} else {
						forceNewChunk = 0;
					}
				}
				//update our offsets...
				if (self_contained) {
					e = stbl_SetChunkAndOffset(curWriter->stbl, curWriter->sampleNumber, descIndex, curWriter->stsc, &curWriter->stco, offset, forceNewChunk, nb_samp);
					if (e) return e;
					offset += sampSize;
					totSize += sampSize;
				} else {
//					if (curWriter->prev_offset != sampOffset) forceNewChunk = 1;
					curWriter->prev_offset = sampOffset + sampSize;

					//we have a DataRef, so use the offset idicated in sampleToChunk
					//and ChunkOffset tables...
					e = stbl_SetChunkAndOffset(curWriter->stbl, curWriter->sampleNumber, descIndex, curWriter->stsc, &curWriter->stco, sampOffset, chunked_forced, nb_samp);
					if (e) return e;
				}
			} else {
				//this is no game, we're writing ....
				if (self_contained) {
					e = WriteSample(mw, sampSize, sampOffset, stsc_ent->isEdited, bs, 1);
					if (e) return e;
				}
			}
			//ok, the sample is done
			if (curWriter->sampleNumber == curWriter->stbl->SampleSize->sampleCount) {
				curWriter->isDone = 1;
				//one more track done...
				tracksDone ++;
			} else {
				curWriter->sampleNumber += nb_samp;
			}
			prevWriter = curWriter;
		}
		//if all our track are done, break
		if (tracksDone == gf_list_count(writers)) break;
		//go to next group
		curGroupID ++;
	}
	movie->mdat->dataSize = totSize;
	return GF_OK;
}



/*uncomment the following to easily test large file generation. This will prepend 4096*1MByte of 0 before the media data*/
//#define TEST_LARGE_FILES

GF_Err DoInterleave(MovieWriter *mw, GF_List *writers, GF_BitStream *bs, u8 Emulation, u64 StartOffset, Bool drift_inter)
{
	u32 i, tracksDone;
	TrackWriter *tmp, *curWriter;
	GF_Err e;
	u32 descIndex, sampSize, chunkNumber;
	u64 DTS;
	u16 curGroupID;
	Bool forceNewChunk, writeGroup;
	GF_StscEntry *stsc_ent;
	//this is used to emulate the write ...
	u64 offset, sampOffset, size, mdatSize;
	u32 count;
	GF_ISOFile *movie = mw->movie;

	mdatSize = 0;

#ifdef TEST_LARGE_FILES
	if (!Emulation) {
		char *blank;
		u32 count, i;
		i = count = 0;
		blank = gf_malloc(sizeof(char)*1024*1024);
		memset(blank, 0, sizeof(char)*1024*1024);
		count = 4096;
		memset(blank, 0, sizeof(char)*1024*1024);
		while (i<count) {
			u32 res = gf_bs_write_data(bs, blank, 1024*1024);
			if (res != 1024*1024) fprintf(stderr, "error writing to disk: only %d bytes written\n", res);
			i++;
			fprintf(stderr, "writing blank block: %.02f done - %d/%d \r", (100.0*i)/count , i, count);
		}
		gf_free(blank);
	}
	mdatSize = 4096*1024;
	mdatSize *= 1024;
#endif

	/*write meta content first - WE DON'T support fragmentation of resources in ISOM atm*/
	if (movie->meta) {
		e = DoWriteMeta(movie, movie->meta, bs, Emulation, StartOffset, &size);
		if (e) return e;
		mdatSize += size;
		StartOffset += (u32) size;
	}
	if (movie->moov) {
		if (movie->moov->meta) {
			e = DoWriteMeta(movie, movie->moov->meta, bs, Emulation, StartOffset, &size);
			if (e) return e;
			mdatSize += size;
			StartOffset += (u32) size;
		}
		i=0;
		while ((tmp = (TrackWriter*)gf_list_enum(writers, &i))) {
			if (tmp->mdia->mediaTrack->meta) {
				e = DoWriteMeta(movie, tmp->mdia->mediaTrack->meta, bs, Emulation, StartOffset, &size);
				if (e) return e;
				mdatSize += size;
				StartOffset += (u32) size;
			}
		}
	}


	if (movie->storageMode == GF_ISOM_STORE_TIGHT)
		return DoFullInterleave(mw, writers, bs, Emulation, StartOffset);

	curGroupID = 1;
	//we emulate a write from this offset...
	offset = StartOffset;
	tracksDone = 0;

#ifdef TEST_LARGE_FILES
	offset += mdatSize;
#endif

	count = gf_list_count(writers);
	//browse each groups
	while (1) {
		/*the max DTS the chunk of the current writer*/
		u64 chunkLastDTS = 0;
		/*the timescale related to the max DTS*/
		u32 chunkLastScale = 0;

		writeGroup = 1;

		//proceed a group
		while (writeGroup) {
			curWriter = NULL;
			for (i=0 ; i < count; i++) {
				tmp = (TrackWriter*)gf_list_get(writers, i);

				//is it done writing ?
				if (tmp->isDone) continue;

				//is it in our group ??
				if (tmp->stbl->groupID != curGroupID) continue;

				//write till this chunk is full on this track...
				while (1) {
					Bool self_contained;
					u32 nb_samp = 1;
					//To Check: are empty sample tables allowed ???
					if (tmp->sampleNumber > tmp->stbl->SampleSize->sampleCount) {
						tmp->isDone = 1;
						tracksDone ++;
						break;
					}

					//OK, get the current sample in this track
					stbl_GetSampleDTS(tmp->stbl->TimeToSample, tmp->sampleNumber, &DTS);

					//can this sample fit in our chunk ?
					if ( ( (DTS - tmp->DTSprev) + tmp->chunkDur) *  movie->moov->mvhd->timeScale > movie->interleavingTime * tmp->timeScale
					        /*drift check: reject sample if outside our check window*/
					        || (drift_inter && chunkLastDTS && ( ((u64)tmp->DTSprev*chunkLastScale) > ((u64)chunkLastDTS*tmp->timeScale)) )
					   ) {
						//in case the sample is longer than InterleaveTime
						if (!tmp->chunkDur) {
							forceNewChunk = 1;
						} else {
							//this one is full. go to next one (exit the loop)
							tmp->chunkDur = 0;
							break;
						}
					} else {
						forceNewChunk = tmp->chunkDur ? 0 : 1;
					}
					//OK, we can write this track
					curWriter = tmp;

					//small check for first 2 samples (DTS = 0 :)
					if (tmp->sampleNumber == 2 && !tmp->chunkDur) forceNewChunk = 0;

					tmp->chunkDur += (u32) (DTS - tmp->DTSprev);
					tmp->DTSprev = DTS;

					e = stbl_GetSampleInfos(curWriter->stbl, curWriter->sampleNumber, &sampOffset, &chunkNumber, &descIndex, &stsc_ent);
					if (e)
						return e;
					e = stbl_GetSampleSize(curWriter->stbl->SampleSize, curWriter->sampleNumber, &sampSize);
					if (e)
						return e;

					self_contained = ((curWriter->all_dref_mode==ISOM_DREF_SELF) || Media_IsSelfContained(curWriter->mdia, descIndex)) ? GF_TRUE : GF_FALSE;

					update_writer_constant_dur(movie, curWriter, stsc_ent, &nb_samp, &sampSize, GF_FALSE);

					if (curWriter->stbl->MaxChunkSize && (curWriter->chunkSize + sampSize > curWriter->stbl->MaxChunkSize)) {
						curWriter->chunkSize = 0;
						forceNewChunk = 1;
					}
					curWriter->chunkSize += sampSize;

					//do we actually write, or do we emulate ?
					if (Emulation) {
						//update our offsets...
						if (self_contained) {
							e = stbl_SetChunkAndOffset(curWriter->stbl, curWriter->sampleNumber, descIndex, curWriter->stsc, &curWriter->stco, offset, forceNewChunk, nb_samp);
							if (e)
								return e;
							offset += sampSize;
							mdatSize += sampSize;
						} else {
							if (curWriter->prev_offset != sampOffset) forceNewChunk = 1;
							curWriter->prev_offset = sampOffset + sampSize;

							//we have a DataRef, so use the offset idicated in sampleToChunk
							//and ChunkOffset tables...
							e = stbl_SetChunkAndOffset(curWriter->stbl, curWriter->sampleNumber, descIndex, curWriter->stsc, &curWriter->stco, sampOffset, forceNewChunk, nb_samp);
							if (e) return e;
						}
					} else {
						//we're writing ....
						if (self_contained) {
							e = WriteSample(mw, sampSize, sampOffset, stsc_ent->isEdited, bs, nb_samp);
							if (e)
								return e;
						}
					}
					//ok, the sample is done
					if (curWriter->sampleNumber >= curWriter->stbl->SampleSize->sampleCount) {
						curWriter->isDone = 1;
						//one more track done...
						tracksDone ++;
						break;
					} else {
						curWriter->sampleNumber += nb_samp;
					}
				}
				/*record chunk end-time & track timescale for drift-controled interleaving*/
				if (drift_inter && curWriter) {
					chunkLastScale = curWriter->timeScale;
					chunkLastDTS = curWriter->DTSprev;
					/*add one interleave window drift - since the "maxDTS" is the previously written one, we will
					have the following cases:
					- sample doesn't fit: post-pone and force new chunk
					- sample time larger than previous chunk time + interleave: post-pone and force new chunk
					- otherwise store and track becomes current reference

					this ensures a proper drift regulation (max DTS diff is less than the interleaving window)
					*/
					chunkLastDTS += curWriter->timeScale * movie->interleavingTime / movie->moov->mvhd->timeScale;
				}
			}
			//no sample found, we're done with this group
			if (!curWriter) {
				writeGroup = 0;
				continue;
			}
		}
		//if all our track are done, break
		if (tracksDone == gf_list_count(writers)) break;
		//go to next group
		curGroupID ++;
	}
	if (movie->mdat) movie->mdat->dataSize = mdatSize;
	return GF_OK;
}


static GF_Err WriteInterleaved(MovieWriter *mw, GF_BitStream *bs, Bool drift_inter)
{
	GF_Err e;
	u32 i;
	s32 moov_meta_pos=-1;
	GF_Box *a, *cprt_box;
	u64 firstSize, finalSize, offset, finalOffset;
	GF_List *writers = gf_list_new();
	GF_ISOFile *movie = mw->movie;

	//first setup the writers
	e = SetupWriters(mw, writers, 1);
	if (e) goto exit;


	if (movie->is_jp2) {
		gf_bs_write_u32(bs, 12);
		gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_JP);
		gf_bs_write_u32(bs, 0x0D0A870A);
	}
	if (movie->brand) {
		e = gf_isom_box_size((GF_Box *)movie->brand);
		if (e) goto exit;
		e = gf_isom_box_write((GF_Box *)movie->brand, bs);
		if (e) goto exit;
	}
	if (movie->pdin) {
		e = gf_isom_box_size((GF_Box *)movie->pdin);
		if (e) goto exit;
		e = gf_isom_box_write((GF_Box *)movie->pdin, bs);
		if (e) goto exit;
	}

	//write all boxes before moov
	i=0;
	while ((a = (GF_Box*)gf_list_enum(movie->TopBoxes, &i))) {
		switch (a->type) {
		case GF_ISOM_BOX_TYPE_MOOV:
		case GF_ISOM_BOX_TYPE_META:
			moov_meta_pos = i-1;
			break;
		case GF_ISOM_BOX_TYPE_FTYP:
		case GF_ISOM_BOX_TYPE_PDIN:
		case GF_ISOM_BOX_TYPE_MDAT:
			break;
		case GF_ISOM_BOX_TYPE_FREE:
			//for backward compat with old arch, keep copyright before moov
			if (((GF_FreeSpaceBox*)a)->dataSize>4) {
				GF_FreeSpaceBox *fr = (GF_FreeSpaceBox*) a;
				if ((fr->dataSize>20) && !strncmp(fr->data, "IsoMedia File", 13)) {
					cprt_box = a;
					break;
				}
			}
		default:
			if (moov_meta_pos<0) {
				e = gf_isom_box_size(a);
				if (e) goto exit;
				e = gf_isom_box_write(a, bs);
				if (e) goto exit;
			}
			break;
		}
	}

	e = DoInterleave(mw, writers, bs, 1, gf_bs_get_position(bs), drift_inter);
	if (e) goto exit;

	firstSize = GetMoovAndMetaSize(movie, writers);
	offset = firstSize;
	if (movie->mdat && movie->mdat->dataSize) offset += 8 + (movie->mdat->dataSize > 0xFFFFFFFF ? 8 : 0);
	e = ShiftOffset(movie, writers, offset);
	if (e) goto exit;
	//get the size and see if it has changed (eg, we moved to 64 bit offsets)
	finalSize = GetMoovAndMetaSize(movie, writers);
	if (firstSize != finalSize) {
		finalOffset = finalSize;
		if (movie->mdat && movie->mdat->dataSize) finalOffset += 8 + (movie->mdat->dataSize > 0xFFFFFFFF ? 8 : 0);
		//OK, now we're sure about the final size -> shift the offsets
		//we don't need to re-emulate, as the only thing that changed is the offset
		//so just shift the offset
		e = ShiftOffset(movie, writers, finalOffset - offset);
		if (e) goto exit;
		/*firstSize = */GetMoovAndMetaSize(movie, writers);
	}
	//now write our stuff
	e = WriteMoovAndMeta(movie, writers, bs);
	if (e) goto exit;

	/*we have 8 extra bytes for large size (not computed in gf_isom_box_size) */
	if (movie->mdat && movie->mdat->dataSize) {
		if (movie->mdat->dataSize > 0xFFFFFFFF) movie->mdat->dataSize += 8;
		e = gf_isom_box_size((GF_Box *)movie->mdat);
		if (e) goto exit;
		e = gf_isom_box_write((GF_Box *)movie->mdat, bs);
		if (e) goto exit;
	}

	//we don't need the offset as we are writing...
	ResetWriters(writers);
	e = DoInterleave(mw, writers, bs, 0, 0, drift_inter);
	if (e) goto exit;


	//then the rest
	i=0;
	while ((a = (GF_Box*)gf_list_enum(movie->TopBoxes, &i))) {
		if ((i-1 < (u32) moov_meta_pos) && (a != cprt_box))
			continue;
		switch (a->type) {
		case GF_ISOM_BOX_TYPE_MOOV:
		case GF_ISOM_BOX_TYPE_META:
		case GF_ISOM_BOX_TYPE_FTYP:
		case GF_ISOM_BOX_TYPE_PDIN:
		case GF_ISOM_BOX_TYPE_MDAT:
			break;
		default:
			e = gf_isom_box_size(a);
			if (e) goto exit;
			e = gf_isom_box_write(a, bs);
			if (e) goto exit;
		}
	}

exit:
	CleanWriters(writers);
	gf_list_del(writers);
	return e;
}

extern u32 default_write_buffering_size;

GF_Err WriteToFile(GF_ISOFile *movie, Bool for_fragments)
{
	FILE *stream;
	GF_BitStream *bs;
	MovieWriter mw;
	GF_Err e = GF_OK;
	if (!movie) return GF_BAD_PARAM;

	if (movie->openMode == GF_ISOM_OPEN_READ) return GF_BAD_PARAM;

	e = gf_isom_insert_copyright(movie);
	if (e) return e;

	memset(&mw, 0, sizeof(mw));
	mw.movie = movie;

	if (gf_sys_is_test_mode() && movie->moov) {
		u32 i;
		GF_TrackBox *trak;
		movie->moov->mvhd->creationTime = 0;
		movie->moov->mvhd->modificationTime = 0;
		i=0;
		while ( (trak = gf_list_enum(movie->moov->trackList, &i))) {
			trak->Header->creationTime = 0;
			trak->Header->modificationTime = 0;
			if (trak->Media->handler->nameUTF8 && strstr(trak->Media->handler->nameUTF8, "@GPAC")) {
				gf_free(trak->Media->handler->nameUTF8);
				trak->Media->handler->nameUTF8 = gf_strdup("MediaHandler");
			}
			trak->Media->mediaHeader->creationTime = 0;
			trak->Media->mediaHeader->modificationTime = 0;
		}
	}

	//capture mode: we don't need a new bitstream
	if (movie->openMode == GF_ISOM_OPEN_WRITE) {
		if (!strcmp(movie->fileName, "_gpac_isobmff_redirect")) {
			GF_BitStream *bs;
			u64 mdat_end = gf_bs_get_position(movie->editFileMap->bs);
			u64 mdat_start = movie->mdat->bsOffset;
			u64 mdat_size = mdat_end - mdat_start;

			if (for_fragments) {
				if (!movie->on_block_out) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] Missing output block callback, cannot write\n"));
					return GF_BAD_PARAM;
				}
				bs = gf_bs_new_cbk(movie->on_block_out, movie->on_block_out_usr_data, movie->on_block_out_block_size);
				e = WriteFlat(&mw, 0, bs, GF_TRUE, GF_TRUE);
				movie->fragmented_file_pos = gf_bs_get_position(bs);
				gf_bs_del(bs);
				return e;
			}
			//write as non-seekable
			e = WriteFlat(&mw, 0, movie->editFileMap->bs, GF_TRUE, GF_FALSE);

			movie->fragmented_file_pos = gf_bs_get_position(movie->editFileMap->bs);
			if (mdat_start && mdat_size) {
				char data[16];
				if (!movie->on_block_out) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] Missing output block patch callback, cannot patch mdat size in flat storage\n"));
					return GF_BAD_PARAM;
				}

				//create a patch packet for mdat covering out 16 bytes (cf FlushCapture)
				bs = gf_bs_new(data, 16, GF_BITSTREAM_WRITE);
				gf_bs_write_u32(bs, (mdat_size>0xFFFFFFFF) ? 1 : (u32) mdat_size);
				gf_bs_write_u32(bs, GF_ISOM_BOX_TYPE_MDAT);
				if  (mdat_size>0xFFFFFFFF)
					gf_bs_write_u64(bs, mdat_size);
				else
					gf_bs_write_u64(bs, 0);
				gf_bs_del(bs);
				movie->on_block_patch(movie->on_block_out_usr_data, data, 16, mdat_start);
			}
		} else {
			e = WriteFlat(&mw, 0, movie->editFileMap->bs, GF_FALSE, GF_FALSE);
		}
	} else {
		u32 buffer_size = movie->editFileMap ? gf_bs_get_output_buffering(movie->editFileMap->bs) : 0;
		Bool is_stdout = GF_FALSE;
		if (!strcmp(movie->finalName, "_gpac_isobmff_redirect")) {
			if (!movie->on_block_out) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[ISOBMFF] Missing output block callback, cannot write\n"));
				return GF_BAD_PARAM;
			}
			bs = gf_bs_new_cbk(movie->on_block_out, movie->on_block_out_usr_data, movie->on_block_out_block_size);
			is_stdout = GF_TRUE;
		} else {
			if (!strcmp(movie->finalName, "std"))
				is_stdout = GF_TRUE;

			//OK, we need a new bitstream
			stream = is_stdout ? stdout : gf_fopen(movie->finalName, "w+b");
			if (!stream)
				return GF_IO_ERR;
			bs = gf_bs_from_file(stream, GF_BITSTREAM_WRITE);
		}
		if (!bs) {
			if (!is_stdout)
				gf_fclose(stream);
			return GF_OUT_OF_MEM;
		}

		if (buffer_size) {
			gf_bs_set_output_buffering(bs, buffer_size);
		}

		switch (movie->storageMode) {
		case GF_ISOM_STORE_TIGHT:
		case GF_ISOM_STORE_INTERLEAVED:
			e = WriteInterleaved(&mw, bs, 0);
			break;
		case GF_ISOM_STORE_DRIFT_INTERLEAVED:
			e = WriteInterleaved(&mw, bs, 1);
			break;
		case GF_ISOM_STORE_STREAMABLE:
			e = WriteFlat(&mw, 1, bs, is_stdout, GF_FALSE);
			break;
		default:
			e = WriteFlat(&mw, 0, bs, is_stdout, GF_FALSE);
			break;
		}

		gf_bs_del(bs);
		if (!is_stdout)
			gf_fclose(stream);
	}
	if (mw.buffer) gf_free(mw.buffer);
	if (mw.nb_done<mw.total_samples) {
		mw.nb_done = mw.total_samples;
		muxer_report_progress(&mw);
	}
	return e;
}



#endif	/*!defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)*/
