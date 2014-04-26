
/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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

#define GPAC_ISOM_CPRT_NOTICE "IsoMedia File Produced with GPAC "GPAC_FULL_VERSION

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
				if (!strcmp(_free->data, GPAC_ISOM_CPRT_NOTICE)) return GF_OK;
				if (strstr(_free->data, "File Produced with GPAC")) {
					gf_free(_free->data);
					_free->data = gf_strdup(GPAC_ISOM_CPRT_NOTICE);
					_free->dataSize = (u32) strlen(_free->data);
					return GF_OK;
				}
			}
		}
	}
	a = gf_isom_box_new(GF_ISOM_BOX_TYPE_FREE);
	if (!a) return GF_OUT_OF_MEM;
	_free = (GF_FreeSpaceBox *)a;
	_free->dataSize = (u32) strlen(GPAC_ISOM_CPRT_NOTICE) + 1;
	_free->data = gf_strdup(GPAC_ISOM_CPRT_NOTICE);
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
	u64 DTSprev;
	u8 isDone;
	u64 prev_offset;
	GF_MediaBox *mdia;
	/*each writer has a sampleToChunck and ChunkOffset tables
	these tables are filled during emulation mode and then will	replace the table in the GF_SampleTableBox*/
	GF_SampleToChunkBox *stsc;
	/*we don't know if it's a large offset or not*/
	GF_Box *stco;
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
		writer->timeScale = trak->Media->mediaHeader->timeScale;
		writer->isDone = 0;
		writer->DTSprev = 0;
		writer->chunkDur = 0;
		writer->stsc = (GF_SampleToChunkBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_STSC);
		if (trak->Media->information->sampleTable->ChunkOffset->type == GF_ISOM_BOX_TYPE_STCO) {
			writer->stco = gf_isom_box_new(GF_ISOM_BOX_TYPE_STCO);
		} else {
			writer->stco = gf_isom_box_new(GF_ISOM_BOX_TYPE_CO64);
		}
		/*stops from chunk escape*/
		if (interleaving) writer->mdia->information->sampleTable->MaxSamplePerChunk = 0;
		/*for progress, assume only one descIndex*/
		if (Media_IsSelfContained(writer->mdia, 1)) mw->total_samples += trak->Media->information->sampleTable->SampleSize->sampleCount;
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
	GF_ChunkOffsetBox *stco;
	GF_ChunkLargeOffsetBox *co64;

	if (file->meta) ShiftMetaOffset(file->meta, offset);
	if (file->moov && file->moov->meta) ShiftMetaOffset(file->moov->meta, offset);

	i=0;
	while ((writer = (TrackWriter *)gf_list_enum(writers, &i))) {
		if (writer->mdia->mediaTrack->meta) ShiftMetaOffset(writer->mdia->mediaTrack->meta, offset);

		//we have to proceed entry by entry in case a part of the media is not self-contained...
		for (j=0; j<writer->stsc->nb_entries; j++) {
			ent = &writer->stsc->entries[j];
			if (!Media_IsSelfContained(writer->mdia, ent->sampleDescriptionIndex)) continue;

			//OK, get the chunk(s) number(s) and "shift" its (their) offset(s).
			if (writer->stco->type == GF_ISOM_BOX_TYPE_STCO) {
				stco = (GF_ChunkOffsetBox *) writer->stco;
				//be carefull for the last entry, nextChunk is set to 0 in edit mode...
				last = ent->nextChunk ? ent->nextChunk : stco->nb_entries + 1;
				for (k = ent->firstChunk; k < last; k++) {

					if (stco->offsets[k-1] + offset > 0xFFFFFFFF) {
						//too bad, rewrite the table....
						co64 = (GF_ChunkLargeOffsetBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_CO64);
						if (!co64) return GF_OUT_OF_MEM;
						co64->nb_entries = stco->nb_entries;
						co64->offsets = (u64*)gf_malloc(co64->nb_entries * sizeof(u64));
						if (!co64) {
							gf_isom_box_del((GF_Box *)co64);
							return GF_OUT_OF_MEM;
						}
						//duplicate the table
						for (l = 0; l < co64->nb_entries; l++) {
							co64->offsets[l] = (u64) stco->offsets[l];
							if (l + 1 == k) co64->offsets[l] += offset;
						}
						//and replace our box
						gf_isom_box_del(writer->stco);
						writer->stco = (GF_Box *)co64;
					} else {
						stco->offsets[k-1] += (u32) offset;
					}
				}
			} else {
				co64 = (GF_ChunkLargeOffsetBox *) writer->stco;
				//be carefull for the last entry ...
				last = ent->nextChunk ? ent->nextChunk : co64->nb_entries + 1;
				for (k = ent->firstChunk; k < last; k++) {
					co64->offsets[k-1] += offset;
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
		//switch all our tables
		i=0;
		while ((writer = (TrackWriter*)gf_list_enum(writers, &i))) {
			//don't delete them !!!
			stsc = writer->mdia->information->sampleTable->SampleToChunk;
			stco = writer->mdia->information->sampleTable->ChunkOffset;
			writer->mdia->information->sampleTable->SampleToChunk = writer->stsc;
			writer->mdia->information->sampleTable->ChunkOffset = writer->stco;
			writer->stco = stco;
			writer->stsc = stsc;
		}
		//write the moov box...
		e = gf_isom_box_size((GF_Box *)movie->moov);
		if (e) return e;
		e = gf_isom_box_write((GF_Box *)movie->moov, bs);
		//and re-switch our table. We have to do it that way because it is
		//needed when the moov is written first
		i=0;
		while ((writer = (TrackWriter*)gf_list_enum(writers, &i))) {
			//don't delete them !!!
			stsc = writer->stsc;
			stco = writer->stco;
			writer->stsc = writer->mdia->information->sampleTable->SampleToChunk;
			writer->stco = writer->mdia->information->sampleTable->ChunkOffset;
			writer->mdia->information->sampleTable->SampleToChunk = stsc;
			writer->mdia->information->sampleTable->ChunkOffset = stco;
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
			size -= writer->mdia->information->sampleTable->ChunkOffset->size;
			size -= writer->mdia->information->sampleTable->SampleToChunk->size;
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

//Write a sample to the file - this is only called for self-contained media
GF_Err WriteSample(MovieWriter *mw, u32 size, u64 offset, u8 isEdited, GF_BitStream *bs)
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

	mw->nb_done++;
	gf_set_progress("ISO File Writing", mw->nb_done, mw->total_samples);
	return GF_OK;
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
			if (!entry->extent_length && !entry->original_extent_offset) {
				entry->extent_offset = 0;
				continue;
			}
		}

		it_size = 0;
		/*for self contained only*/
		if (!iloc->data_reference_index) {
			iloc->base_offset = baseOffset;

			/*new resource*/
			if (iinf->full_path) {
				FILE *src=NULL;

				if (!iinf->data_len) {
					src = gf_f64_open(iinf->full_path, "rb");
					if (!src) continue;
					gf_f64_seek(src, 0, SEEK_END);
					it_size = gf_f64_tell(src);
					gf_f64_seek(src, 0, SEEK_SET);
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
				if (src) fclose(src);
			}
			else if (gf_list_count(iloc->extent_entries)) {
				u32 j;
				j=0;
				while ((entry = (GF_ItemExtentEntry *)gf_list_enum(iloc->extent_entries, &j))) {
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
	u8 isEdited, force;
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
			//To Check: are empty sample tables allowed ???
			if (writer->sampleNumber > writer->mdia->information->sampleTable->SampleSize->sampleCount) {
				writer->isDone = 1;
				continue;
			}
			e = stbl_GetSampleInfos(writer->mdia->information->sampleTable, writer->sampleNumber, &sampOffset, &chunkNumber, &descIndex, &isEdited);
			if (e) return e;
			e = stbl_GetSampleSize(writer->mdia->information->sampleTable->SampleSize, writer->sampleNumber, &sampSize);
			if (e) return e;

			//update our chunks.
			force = 0;
			if (movie->openMode == GF_ISOM_OPEN_WRITE) {
				offset = sampOffset;
				if (predOffset != offset)
					force = 1;
			}
			//update our global offset...
			if (Media_IsSelfContained(writer->mdia, descIndex) ) {
				e = stbl_SetChunkAndOffset(writer->mdia->information->sampleTable, writer->sampleNumber, descIndex, writer->stsc, &writer->stco, offset, force);
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
				e = stbl_SetChunkAndOffset(writer->mdia->information->sampleTable, writer->sampleNumber, descIndex, writer->stsc, &writer->stco, sampOffset, force);
				if (e) return e;
			}
			//we write the sample if not emulation
			if (!Emulation) {
				if (Media_IsSelfContained(writer->mdia, descIndex) ) {
					e = WriteSample(mw, sampSize, sampOffset, isEdited, bs);
					if (e) return e;
				}
			}
			//ok, the track is done
			if (writer->sampleNumber == writer->mdia->information->sampleTable->SampleSize->sampleCount) {
				writer->isDone = 1;
			} else {
				writer->sampleNumber ++;
			}
		}
	}
	//set the mdatSize...
	movie->mdat->dataSize = mdatSize;
	return GF_OK;
}


//write the file track by track, with moov box before or after the mdat
GF_Err WriteFlat(MovieWriter *mw, u8 moovFirst, GF_BitStream *bs)
{
	GF_Err e;
	u32 i;
	u64 offset, finalOffset, totSize, begin, firstSize, finalSize;
	GF_Box *a;
	GF_List *writers = gf_list_new();
	GF_ISOFile *movie = mw->movie;

	begin = totSize = 0;

	//first setup the writers
	e = SetupWriters(mw, writers, 0);
	if (e) goto exit;

	if (!moovFirst) {
		if (movie->openMode == GF_ISOM_OPEN_WRITE) {
			begin = 0;
			totSize = gf_isom_datamap_get_offset(movie->editFileMap);
			/*start boxes have not been written yet, do it*/
			if (!totSize) {
				if (movie->is_jp2) {
					gf_bs_write_u32(movie->editFileMap->bs, 12);
					gf_bs_write_u32(movie->editFileMap->bs, GF_4CC('j','P',' ',' '));
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
		} else {
			if (movie->is_jp2) {
				gf_bs_write_u32(bs, 12);
				gf_bs_write_u32(bs, GF_4CC('j','P',' ',' '));
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
				//to avoid computing the size each time write always 4 + 4 + 8 bytes before
				begin = gf_bs_get_position(bs);
				gf_bs_write_u64(bs, 0);
				gf_bs_write_u64(bs, 0);
				e = DoWrite(mw, writers, bs, 0, gf_bs_get_position(bs));
				if (e) goto exit;
				totSize = gf_bs_get_position(bs) - begin;
				break;
			default:
				e = gf_isom_box_size(a);
				if (e) goto exit;
				e = gf_isom_box_write(a, bs);
				if (e) goto exit;
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
		}
		movie->mdat->size = totSize;
		goto exit;
	}

	//nope, we have to write the moov first. The pb is that
	//1 - we don't know its size till the mdat is written
	//2 - we don't know the ofset at which the mdat will start...
	//3 - once the mdat is written, the chunkOffset table can have changed...

	if (movie->is_jp2) {
		gf_bs_write_u32(bs, 12);
		gf_bs_write_u32(bs, GF_4CC('j','P',' ',' '));
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
	//What we will do is first emulate the write from the begining...
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
		//we need to remove our offsets
		ResetWriters(writers);
		//finalOffset = (finalSize > 0xFFFFFFFF ? finalSize + 8 : finalSize) + 8 + (movie->mdat->dataSize > 0xFFFFFFFF ? 8 : 0);
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
	u8 forceNewChunk, writeGroup, isEdited;
	//this is used to emulate the write ...
	u64 offset, totSize, sampOffset;
	GF_ISOFile *movie = mw->movie;
	e = GF_OK;


	totSize = 0;
	curGroupID = 1;

	prevWriter = NULL;
	//we emulate a write from this offset...
	offset = StartOffset;
	writeGroup = 1;
	tracksDone = 0;


	//browse each groups
	while (1) {
		writeGroup = 1;

		//proceed a group
		while (writeGroup) {
			//first get the appropriated sample for the min time in this group
			curWriter = NULL;
			DTStmp = (u64) -1;
			TStmp = 0;
			curTrackPriority = (u16) -1;

			i=0;
			while ((tmp = (TrackWriter*)gf_list_enum(writers, &i))) {

				//is it done writing ?
				//is it in our group ??
				if (tmp->isDone || tmp->mdia->information->sampleTable->groupID != curGroupID) continue;

				//OK, get the current sample in this track
				stbl_GetSampleDTS(tmp->mdia->information->sampleTable->TimeToSample, tmp->sampleNumber, &DTS);
				res = TStmp ? DTStmp * tmp->timeScale - DTS * TStmp : 0;
				if (res < 0) continue;
				if ((!res) && curTrackPriority <= tmp->mdia->information->sampleTable->trackPriority) continue;
				curWriter = tmp;
				curTrackPriority = tmp->mdia->information->sampleTable->trackPriority;
				DTStmp = DTS;
				TStmp = tmp->timeScale;
			}
			//no sample found, we're done with this group
			if (!curWriter) {
				//we're done with the group
				curTrackPriority = 0;
				writeGroup = 0;
				continue;
			}
			//To Check: are empty sample tables allowed ???
			if (curWriter->sampleNumber > curWriter->mdia->information->sampleTable->SampleSize->sampleCount) {
				curWriter->isDone = 1;
				tracksDone ++;
				continue;
			}

			e = stbl_GetSampleInfos(curWriter->mdia->information->sampleTable, curWriter->sampleNumber, &sampOffset, &chunkNumber, &descIndex, &isEdited);
			if (e) return e;
			e = stbl_GetSampleSize(curWriter->mdia->information->sampleTable->SampleSize, curWriter->sampleNumber, &sampSize);
			if (e) return e;

			//do we actually write, or do we emulate ?
			if (Emulation) {
				//are we in the same track ??? If not, force a new chunk when adding this sample
				if (curWriter != prevWriter) {
					forceNewChunk = 1;
				} else {
					forceNewChunk = 0;
				}
				//update our offsets...
				if (Media_IsSelfContained(curWriter->mdia, descIndex) ) {
					e = stbl_SetChunkAndOffset(curWriter->mdia->information->sampleTable, curWriter->sampleNumber, descIndex, curWriter->stsc, &curWriter->stco, offset, forceNewChunk);
					if (e) return e;
					offset += sampSize;
					totSize += sampSize;
				} else {
					if (curWriter->prev_offset != sampOffset) forceNewChunk = 1;
					curWriter->prev_offset = sampOffset + sampSize;

					//we have a DataRef, so use the offset idicated in sampleToChunk
					//and ChunkOffset tables...
					e = stbl_SetChunkAndOffset(curWriter->mdia->information->sampleTable, curWriter->sampleNumber, descIndex, curWriter->stsc, &curWriter->stco, sampOffset, 0);
					if (e) return e;
				}
			} else {
				//this is no game, we're writing ....
				if (Media_IsSelfContained(curWriter->mdia, descIndex) ) {
					e = WriteSample(mw, sampSize, sampOffset, isEdited, bs);
					if (e) return e;
				}
			}
			//ok, the sample is done
			if (curWriter->sampleNumber == curWriter->mdia->information->sampleTable->SampleSize->sampleCount) {
				curWriter->isDone = 1;
				//one more track done...
				tracksDone ++;
			} else {
				curWriter->sampleNumber ++;
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
	u8 forceNewChunk, writeGroup, isEdited;
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
	if (movie->moov && movie->moov->meta) {
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



	if (movie->storageMode == GF_ISOM_STORE_TIGHT)
		return DoFullInterleave(mw, writers, bs, Emulation, StartOffset);

	e = GF_OK;

	curGroupID = 1;
	//we emulate a write from this offset...
	offset = StartOffset;
	writeGroup = 1;
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
				if (tmp->mdia->information->sampleTable->groupID != curGroupID) continue;

				//write till this chunk is full on this track...
				while (1) {
					//To Check: are empty sample tables allowed ???
					if (tmp->sampleNumber > tmp->mdia->information->sampleTable->SampleSize->sampleCount) {
						tmp->isDone = 1;
						tracksDone ++;
						break;
					}

					//OK, get the current sample in this track
					stbl_GetSampleDTS(tmp->mdia->information->sampleTable->TimeToSample, tmp->sampleNumber, &DTS);

					//can this sample fit in our chunk ?
					if ( ( (DTS - tmp->DTSprev) + tmp->chunkDur) *  movie->moov->mvhd->timeScale > movie->interleavingTime * tmp->timeScale
					        /*drfit check: reject sample if outside our check window*/
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

					e = stbl_GetSampleInfos(curWriter->mdia->information->sampleTable, curWriter->sampleNumber, &sampOffset, &chunkNumber, &descIndex, &isEdited);
					if (e) return e;
					e = stbl_GetSampleSize(curWriter->mdia->information->sampleTable->SampleSize, curWriter->sampleNumber, &sampSize);
					if (e) return e;

					//do we actually write, or do we emulate ?
					if (Emulation) {
						//update our offsets...
						if (Media_IsSelfContained(curWriter->mdia, descIndex) ) {
							e = stbl_SetChunkAndOffset(curWriter->mdia->information->sampleTable, curWriter->sampleNumber, descIndex, curWriter->stsc, &curWriter->stco, offset, forceNewChunk);
							if (e) return e;
							offset += sampSize;
							mdatSize += sampSize;
						} else {
							if (curWriter->prev_offset != sampOffset) forceNewChunk = 1;
							curWriter->prev_offset = sampOffset + sampSize;

							//we have a DataRef, so use the offset idicated in sampleToChunk
							//and ChunkOffset tables...
							e = stbl_SetChunkAndOffset(curWriter->mdia->information->sampleTable, curWriter->sampleNumber, descIndex, curWriter->stsc, &curWriter->stco, sampOffset, forceNewChunk);
							if (e) return e;
						}
					} else {
						//this is no game, we're writing ....
						if (Media_IsSelfContained(curWriter->mdia, descIndex) ) {
							e = WriteSample(mw, sampSize, sampOffset, isEdited, bs);
							if (e) return e;
						}
					}
					//ok, the sample is done
					if (curWriter->sampleNumber == curWriter->mdia->information->sampleTable->SampleSize->sampleCount) {
						curWriter->isDone = 1;
						//one more track done...
						tracksDone ++;
						break;
					} else {
						curWriter->sampleNumber ++;
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
	GF_Box *a;
	u64 firstSize, finalSize, offset, finalOffset;
	GF_List *writers = gf_list_new();
	GF_ISOFile *movie = mw->movie;

	//first setup the writers
	e = SetupWriters(mw, writers, 1);
	if (e) goto exit;


	if (movie->is_jp2) {
		gf_bs_write_u32(bs, 12);
		gf_bs_write_u32(bs, GF_4CC('j','P',' ',' '));
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
		//we need to remove our offsets
		ResetWriters(writers);
		finalOffset = finalSize;
		if (movie->mdat->dataSize) finalOffset += 8 + (movie->mdat->dataSize > 0xFFFFFFFF ? 8 : 0);
		//OK, now we're sure about the final size -> shift the offsets
		//we don't need to re-emulate, as the only thing that changed is the offset
		//so just shift the offset
		e = ShiftOffset(movie, writers, finalOffset - offset);
		if (e) goto exit;
		firstSize = GetMoovAndMetaSize(movie, writers);
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

GF_Err WriteToFile(GF_ISOFile *movie)
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

	//capture mode: we don't need a new bitstream
	if (movie->openMode == GF_ISOM_OPEN_WRITE) {
		e = WriteFlat(&mw, 0, movie->editFileMap->bs);
	} else {
		u32 buffer_size = movie->editFileMap ? gf_bs_get_output_buffering(movie->editFileMap->bs) : 0;
		Bool is_stdout = 0;
		if (!strcmp(movie->finalName, "std"))
			is_stdout = 1;

		//OK, we need a new bitstream
		stream = is_stdout ? stdout : gf_f64_open(movie->finalName, "w+b");
		if (!stream)
			return GF_IO_ERR;
		bs = gf_bs_from_file(stream, GF_BITSTREAM_WRITE);
		if (!bs) {
			if (!is_stdout)
				fclose(stream);
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
			e = WriteFlat(&mw, 1, bs);
			break;
		default:
			e = WriteFlat(&mw, 0, bs);
			break;
		}

		gf_bs_del(bs);
		if (!is_stdout)
			fclose(stream);
	}
	if (mw.buffer) gf_free(mw.buffer);
	if (mw.nb_done<mw.total_samples) {
		gf_set_progress("ISO File Writing", mw.total_samples, mw.total_samples);
	}
	return e;
}



#endif	/*!defined(GPAC_DISABLE_ISOM) && !defined(GPAC_DISABLE_ISOM_WRITE)*/
