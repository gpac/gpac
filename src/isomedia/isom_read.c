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
#include <gpac/constants.h>

#ifndef GPAC_DISABLE_ISOM

//the only static var. Used to store any error happening while opening a movie
static GF_Err MP4_API_IO_Err;

void gf_isom_set_last_error(GF_ISOFile *movie, GF_Err error)
{
	if (!movie) {
		MP4_API_IO_Err = error;
	} else {
		movie->LastError = error;
	}
}


GF_EXPORT
GF_Err gf_isom_last_error(GF_ISOFile *the_file)
{
	if (!the_file) return MP4_API_IO_Err;
	return the_file->LastError;
}

GF_EXPORT
u8 gf_isom_get_mode(GF_ISOFile *the_file)
{
	if (!the_file) return 0;
	return the_file->openMode;
}

#if 0 //unused
/*! gets file size of an ISO file
\param isom_file the target ISO file
\return the file size in bytes
*/
u64 gf_isom_get_file_size(GF_ISOFile *the_file)
{
	if (!the_file) return 0;
	if (the_file->movieFileMap) return gf_bs_get_size(the_file->movieFileMap->bs);
#ifndef GPAC_DISABLE_ISOM_WRITE
	if (the_file->editFileMap) return gf_bs_get_size(the_file->editFileMap->bs);
#endif
	return 0;
}
#endif

GF_EXPORT
GF_Err gf_isom_freeze_order(GF_ISOFile *file)
{
	u32 i=0;
	GF_Box *box;
	if (!file) return GF_BAD_PARAM;
	while ((box=gf_list_enum(file->TopBoxes, &i))) {
		gf_isom_box_freeze_order(box);
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_set_inplace_padding(GF_ISOFile *file, u32 padding)
{
	if (!file) return GF_BAD_PARAM;
	file->padding = padding;
	return GF_OK;
}
/**************************************************************
					Sample Manip
**************************************************************/

//creates a new empty sample
GF_EXPORT
GF_ISOSample *gf_isom_sample_new()
{
	GF_ISOSample *tmp;
	GF_SAFEALLOC(tmp, GF_ISOSample);
	return tmp;
}

//delete a sample
GF_EXPORT
void gf_isom_sample_del(GF_ISOSample **samp)
{
	if (!samp || ! *samp) return;
	if ((*samp)->data && (*samp)->dataLength) gf_free((*samp)->data);
	gf_free(*samp);
	*samp = NULL;
}

static u32 gf_isom_probe_type(u32 type)
{
	switch (type) {
	case GF_ISOM_BOX_TYPE_FTYP:
	case GF_ISOM_BOX_TYPE_MOOV:
		return 2;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	case GF_ISOM_BOX_TYPE_MOOF:
	case GF_ISOM_BOX_TYPE_STYP:
	case GF_ISOM_BOX_TYPE_SIDX:
	case GF_ISOM_BOX_TYPE_EMSG:
	case GF_ISOM_BOX_TYPE_PRFT:
    //we map free as segment when it is first in the file - a regular file shall start with ftyp or a file sig, not free
    //since our route stack may patch boxes to free for incomplete segments, we must map this to free
    case GF_ISOM_BOX_TYPE_FREE:
		return 3;
#ifndef GPAC_DISABLE_ISOM_ADOBE
	/*Adobe specific*/
	case GF_ISOM_BOX_TYPE_AFRA:
	case GF_ISOM_BOX_TYPE_ABST:
#endif
#endif
	case GF_ISOM_BOX_TYPE_MDAT:
	case GF_ISOM_BOX_TYPE_SKIP:
	case GF_ISOM_BOX_TYPE_UDTA:
	case GF_ISOM_BOX_TYPE_META:
	case GF_ISOM_BOX_TYPE_VOID:
	case GF_ISOM_BOX_TYPE_JP:
	case GF_QT_BOX_TYPE_WIDE:
		return 1;
	default:
		return 0;
	}
}

GF_EXPORT
u32 gf_isom_probe_file_range(const char *fileName, u64 start_range, u64 end_range)
{
	u32 type = 0;

	if (!strncmp(fileName, "gmem://", 7)) {
		u32 size;
		u8 *mem_address;
		if (gf_blob_get(fileName, &mem_address, &size, NULL) != GF_OK) {
			return 0;
		}
        if (size && (size > start_range + 8)) {
			type = GF_4CC(mem_address[start_range + 4], mem_address[start_range + 5], mem_address[start_range + 6], mem_address[start_range + 7]);
        }
        gf_blob_release(fileName);
	} else if (!strncmp(fileName, "isobmff://", 10)) {
		return 2;
	} else {
		u32 nb_read;
		unsigned char data[4];
		FILE *f = gf_fopen(fileName, "rb");
		if (!f) return 0;
		if (start_range) gf_fseek(f, start_range, SEEK_SET);
		type = 0;
		nb_read = (u32) gf_fread(data, 4, f);
		if (nb_read == 4) {
			if (gf_fread(data, 4, f) == 4) {
				type = GF_4CC(data[0], data[1], data[2], data[3]);
			}
		}
		gf_fclose(f);
		if (!nb_read) return 0;
	}
	return gf_isom_probe_type(type);
}

GF_EXPORT
u32 gf_isom_probe_file(const char *fileName)
{
	return gf_isom_probe_file_range(fileName, 0, 0);
}

GF_EXPORT
u32 gf_isom_probe_data(const u8*inBuf, u32 inSize)
{
	u32 type;
	if (inSize < 8) return 0;
	type = GF_4CC(inBuf[4], inBuf[5], inBuf[6], inBuf[7]);
	return gf_isom_probe_type(type);
}


#ifndef GPAC_DISABLE_AV_PARSERS
#include <gpac/internal/media_dev.h>
#endif

static GF_Err isom_create_init_from_mem(const char *fileName, GF_ISOFile *file)
{
	u32 sample_rate=0;
	u32 nb_channels=0;
	u32 bps=0;
	//u32 atag=0;
	u32 nal_len=4;
	u32 width = 0;
	u32 height = 0;
	u32 timescale = 10000000;
	u64 tfdt = 0;
	char sz4cc[5];
	char CodecParams[2048];
	u32 CodecParamLen=0;
	char *sep, *val;
	GF_TrackBox *trak;
	GF_TrackExtendsBox *trex;
	GF_SampleTableBox *stbl;

	sz4cc[0] = 0;

	val = (char*) ( fileName + strlen("isobmff://") );
	while (1)  {
		sep = strchr(val, ' ');
		if (sep) sep[0] = 0;

		if (!strncmp(val, "4cc=", 4)) strcpy(sz4cc, val+4);
		else if (!strncmp(val, "init=", 5)) {
			char szH[3], *data = val+5;
			u32 i, len = (u32) strlen(data);
			for (i=0; i<len; i+=2) {
				u32 v;
				//init is hex-encoded so 2 input bytes for one output char
				szH[0] = data[i];
				szH[1] = data[i+1];
				szH[2] = 0;
				sscanf(szH, "%X", &v);
				CodecParams[CodecParamLen] = (char) v;
				CodecParamLen++;
			}
		}
		else if (!strncmp(val, "nal=", 4)) nal_len = atoi(val+4);
		else if (!strncmp(val, "bps=", 4)) bps = atoi(val+4);
		//else if (!strncmp(val, "atag=", 5)) atag = atoi(val+5);
		else if (!strncmp(val, "ch=", 3)) nb_channels = atoi(val+3);
		else if (!strncmp(val, "srate=", 6)) sample_rate = atoi(val+6);
		else if (!strncmp(val, "w=", 2)) width = atoi(val+2);
		else if (!strncmp(val, "h=", 2)) height = atoi(val+2);
		else if (!strncmp(val, "scale=", 6)) timescale = atoi(val+6);
		else if (!strncmp(val, "tfdt=", 5)) {
			sscanf(val+5, LLX, &tfdt);
		}
		if (!sep) break;
		sep[0] = ' ';
		val = sep+1;
	}
	if (!stricmp(sz4cc, "H264") || !stricmp(sz4cc, "AVC1")) {
	}
	else if (!stricmp(sz4cc, "AACL")) {
	}
	else {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("[iso file] Cannot convert smooth media type %s to ISO init segment\n", sz4cc));
		return GF_NOT_SUPPORTED;
	}

	file->moov = (GF_MovieBox *) gf_isom_box_new(GF_ISOM_BOX_TYPE_MOOV);
	if (!file->moov) return GF_OUT_OF_MEM;
	gf_list_add(file->TopBoxes, file->moov);
	file->moov->mov = file;
	file->is_smooth = GF_TRUE;
	file->moov->mvhd = (GF_MovieHeaderBox *) gf_isom_box_new_parent(&file->moov->child_boxes, GF_ISOM_BOX_TYPE_MVHD);
	if (!file->moov->mvhd) return GF_OUT_OF_MEM;
	file->moov->mvhd->timeScale = timescale;
	file->moov->mvex = (GF_MovieExtendsBox *) gf_isom_box_new_parent(&file->moov->child_boxes, GF_ISOM_BOX_TYPE_MVEX);
	if (!file->moov->mvex) return GF_OUT_OF_MEM;
	trex = (GF_TrackExtendsBox *) gf_isom_box_new_parent(&file->moov->mvex->child_boxes, GF_ISOM_BOX_TYPE_TREX);
	if (!trex) return GF_OUT_OF_MEM;

	trex->def_sample_desc_index = 1;
	trex->trackID = 1;
	gf_list_add(file->moov->mvex->TrackExList, trex);

	trak = (GF_TrackBox *) gf_isom_box_new_parent(&file->moov->child_boxes, GF_ISOM_BOX_TYPE_TRAK);
	if (!trak) return GF_OUT_OF_MEM;
	trak->moov = file->moov;
	gf_list_add(file->moov->trackList, trak);

	trak->Header = (GF_TrackHeaderBox *) gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_TKHD);
	if (!trak->Header) return GF_OUT_OF_MEM;
	trak->Header->trackID = 1;
	trak->Header->flags |= 1;
	trak->Header->width = width;
	trak->Header->height = height;

	trak->Media = (GF_MediaBox *) gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_MDIA);
	if (!trak->Media) return GF_OUT_OF_MEM;
	trak->Media->mediaTrack = trak;
	trak->Media->mediaHeader = (GF_MediaHeaderBox *) gf_isom_box_new_parent(&trak->Media->child_boxes, GF_ISOM_BOX_TYPE_MDHD);
	if (!trak->Media->mediaHeader) return GF_OUT_OF_MEM;
	trak->Media->mediaHeader->timeScale = timescale;

	trak->Media->handler = (GF_HandlerBox *) gf_isom_box_new_parent(&trak->Media->child_boxes,GF_ISOM_BOX_TYPE_HDLR);
	if (!trak->Media->handler) return GF_OUT_OF_MEM;
    //we assume by default vide for handler type (only used for smooth streaming)
	trak->Media->handler->handlerType = width ? GF_ISOM_MEDIA_VISUAL : GF_ISOM_MEDIA_AUDIO;

	trak->Media->information = (GF_MediaInformationBox *) gf_isom_box_new_parent(&trak->Media->child_boxes, GF_ISOM_BOX_TYPE_MINF);
	if (!trak->Media->information) return GF_OUT_OF_MEM;
	trak->Media->information->sampleTable = (GF_SampleTableBox *) gf_isom_box_new_parent(&trak->Media->information->child_boxes, GF_ISOM_BOX_TYPE_STBL);
	if (!trak->Media->information->sampleTable) return GF_OUT_OF_MEM;

	stbl = trak->Media->information->sampleTable;
	stbl->SampleSize = (GF_SampleSizeBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSZ);
	if (!stbl->SampleSize) return GF_OUT_OF_MEM;
	stbl->TimeToSample = (GF_TimeToSampleBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STTS);
	if (!stbl->TimeToSample) return GF_OUT_OF_MEM;
	stbl->ChunkOffset = (GF_Box *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STCO);
	if (!stbl->ChunkOffset) return GF_OUT_OF_MEM;
	stbl->SampleToChunk = (GF_SampleToChunkBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSC);
	if (!stbl->SampleToChunk) return GF_OUT_OF_MEM;
	stbl->SyncSample = (GF_SyncSampleBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSS);
	if (!stbl->SyncSample) return GF_OUT_OF_MEM;
	stbl->SampleDescription = (GF_SampleDescriptionBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSD);
	if (!stbl->SampleDescription) return GF_OUT_OF_MEM;

	trak->dts_at_seg_start = tfdt;
	trak->dts_at_next_frag_start = tfdt;


	if (!stricmp(sz4cc, "H264") || !stricmp(sz4cc, "AVC1")) {
#ifndef GPAC_DISABLE_AV_PARSERS
		u32 pos = 0;
		u32 end, sc_size=0;
#endif
		GF_MPEGVisualSampleEntryBox *avc =  (GF_MPEGVisualSampleEntryBox *) gf_isom_box_new_parent(&stbl->SampleDescription->child_boxes, GF_ISOM_BOX_TYPE_AVC1);
		if (!avc) return GF_OUT_OF_MEM;
		avc->avc_config =  (GF_AVCConfigurationBox *) gf_isom_box_new_parent(&avc->child_boxes, GF_ISOM_BOX_TYPE_AVCC);
		if (!avc->avc_config) return GF_OUT_OF_MEM;

		avc->Width = width;
		avc->Height = height;

		avc->avc_config->config = gf_odf_avc_cfg_new();
		avc->avc_config->config->nal_unit_size = nal_len;
		avc->avc_config->config->configurationVersion = 1;

#ifndef GPAC_DISABLE_AV_PARSERS
		//locate pps and sps
		gf_media_nalu_next_start_code((u8 *) CodecParams, CodecParamLen, &sc_size);
		pos += sc_size;
		while (pos<CodecParamLen) {
			GF_NALUFFParam *slc;
			u8 nal_type;
			char *nal = &CodecParams[pos];
			end = gf_media_nalu_next_start_code(nal, CodecParamLen-pos, &sc_size);
			if (!end) end = CodecParamLen;

			GF_SAFEALLOC(slc, GF_NALUFFParam);
			if (!slc) break;
			slc->size = end;
			slc->data = gf_malloc(sizeof(char)*slc->size);
			if (!slc->data) return GF_OUT_OF_MEM;
			memcpy(slc->data, nal, sizeof(char)*slc->size);

			nal_type = nal[0] & 0x1F;
			if (nal_type == GF_AVC_NALU_SEQ_PARAM) {
/*				AVCState avcc;
				u32 idx = gf_avc_read_sps(slc->data, slc->size, &avcc, 0, NULL);
				avc->avc_config->config->profile_compatibility = avcc.sps[idx].prof_compat;
				avc->avc_config->config->AVCProfileIndication = avcc.sps[idx].profile_idc;
				avc->avc_config->config->AVCLevelIndication = avcc.sps[idx].level_idc;
				avc->avc_config->config->chroma_format = avcc.sps[idx].chroma_format;
				avc->avc_config->config->luma_bit_depth = 8 + avcc.sps[idx].luma_bit_depth_m8;
				avc->avc_config->config->chroma_bit_depth = 8 + avcc.sps[idx].chroma_bit_depth_m8;
*/

				gf_list_add(avc->avc_config->config->sequenceParameterSets, slc);
			} else {
				gf_list_add(avc->avc_config->config->pictureParameterSets, slc);
			}
			pos += slc->size + sc_size;
		}
#endif

		AVC_RewriteESDescriptor(avc);
	}
	else if (!stricmp(sz4cc, "AACL")) {
#ifndef GPAC_DISABLE_AV_PARSERS
		GF_M4ADecSpecInfo aacinfo;
#endif

		GF_MPEGAudioSampleEntryBox *aac =  (GF_MPEGAudioSampleEntryBox *) gf_isom_box_new_parent(&stbl->SampleDescription->child_boxes, GF_ISOM_BOX_TYPE_MP4A);
		if (!aac) return GF_OUT_OF_MEM;
		aac->esd = (GF_ESDBox *) gf_isom_box_new_parent(&aac->child_boxes, GF_ISOM_BOX_TYPE_ESDS);
		if (!aac->esd) return GF_OUT_OF_MEM;
		aac->esd->desc = gf_odf_desc_esd_new(2);
		if (!aac->esd->desc) return GF_OUT_OF_MEM;
#ifndef GPAC_DISABLE_AV_PARSERS
		memset(&aacinfo, 0, sizeof(GF_M4ADecSpecInfo));
		aacinfo.nb_chan = nb_channels;
		aacinfo.base_object_type = GF_M4A_AAC_LC;
		aacinfo.base_sr = sample_rate;
		gf_m4a_write_config(&aacinfo, &aac->esd->desc->decoderConfig->decoderSpecificInfo->data, &aac->esd->desc->decoderConfig->decoderSpecificInfo->dataLength);
#endif
		aac->esd->desc->decoderConfig->streamType = GF_STREAM_AUDIO;
		aac->esd->desc->decoderConfig->objectTypeIndication = GF_CODECID_AAC_MPEG4;
		aac->bitspersample = bps;
		aac->samplerate_hi = sample_rate;
		aac->channel_count = nb_channels;
	}

	return GF_OK;
}

/**************************************************************
					File Opening in streaming mode
			the file map is regular (through FILE handles)
**************************************************************/
GF_EXPORT
GF_Err gf_isom_open_progressive_ex(const char *fileName, u64 start_range, u64 end_range, Bool enable_frag_bounds, GF_ISOFile **the_file, u64 *BytesMissing, u32 *outBoxType)
{
	GF_Err e;
	GF_ISOFile *movie;

	if (!BytesMissing || !the_file)
		return GF_BAD_PARAM;
	*BytesMissing = 0;
	*the_file = NULL;

	movie = gf_isom_new_movie();
	if (!movie) return GF_OUT_OF_MEM;

	movie->fileName = gf_strdup(fileName);
	movie->openMode = GF_ISOM_OPEN_READ;
	movie->signal_frag_bounds = enable_frag_bounds;

#ifndef GPAC_DISABLE_ISOM_WRITE
	movie->editFileMap = NULL;
	movie->finalName = NULL;
#endif /*GPAC_DISABLE_ISOM_WRITE*/

	if (!strncmp(fileName, "isobmff://", 10)) {
		movie->movieFileMap = NULL;
		e = isom_create_init_from_mem(fileName, movie);
	} else {
		//do NOT use FileMapping on incomplete files
		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_READ, &movie->movieFileMap);
		if (e) {
			gf_isom_delete_movie(movie);
			return e;
		}

		if (start_range || end_range) {
			if (end_range>start_range) {
				gf_bs_seek(movie->movieFileMap->bs, end_range+1);
				gf_bs_truncate(movie->movieFileMap->bs);
			}
			gf_bs_seek(movie->movieFileMap->bs, start_range);
		}
		e = gf_isom_parse_movie_boxes(movie, outBoxType, BytesMissing, GF_TRUE);

	}
	if (e == GF_ISOM_INCOMPLETE_FILE) {
		//if we have a moov, we're fine
		if (movie->moov) {
			*the_file = (GF_ISOFile *)movie;
			return GF_OK;
		}
		//if not, delete the movie
		gf_isom_delete_movie(movie);
		return e;
	} else if (e) {
		//if not, delete the movie
		gf_isom_delete_movie(movie);
		return e;
	}

	//OK, let's return
	*the_file = (GF_ISOFile *)movie;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_open_progressive(const char *fileName, u64 start_range, u64 end_range, Bool enable_frag_bounds, GF_ISOFile **the_file, u64 *BytesMissing)
{
	return gf_isom_open_progressive_ex(fileName, start_range, end_range, enable_frag_bounds, the_file, BytesMissing, NULL);
}

/**************************************************************
					File Reading
**************************************************************/

GF_EXPORT
GF_ISOFile *gf_isom_open(const char *fileName, GF_ISOOpenMode OpenMode, const char *tmp_dir)
{
	GF_ISOFile *movie;
	MP4_API_IO_Err = GF_OK;

	switch (OpenMode & 0xFF) {
	case GF_ISOM_OPEN_READ_DUMP:
	case GF_ISOM_OPEN_READ:
		movie = gf_isom_open_file(fileName, OpenMode, NULL);
		break;

#ifndef GPAC_DISABLE_ISOM_WRITE

	case GF_ISOM_OPEN_WRITE:
		movie = gf_isom_create_movie(fileName, OpenMode, tmp_dir);
		break;
	case GF_ISOM_OPEN_EDIT:
	case GF_ISOM_OPEN_READ_EDIT:
	case GF_ISOM_OPEN_KEEP_FRAGMENTS:
		movie = gf_isom_open_file(fileName, OpenMode, tmp_dir);
		break;
	case GF_ISOM_WRITE_EDIT:
		movie = gf_isom_create_movie(fileName, OpenMode, tmp_dir);
		break;

#endif /*GPAC_DISABLE_ISOM_WRITE*/

	default:
		return NULL;
	}
	return (GF_ISOFile *) movie;
}


#if 0
/*! gets access to the data bitstream  - see \ref gf_isom_open
\param isom_file the target ISO file
\param out_bs set to the file input bitstream - do NOT destroy
\return error if any
*/
GF_Err gf_isom_get_bs(GF_ISOFile *movie, GF_BitStream **out_bs)
{
#ifndef GPAC_DISABLE_ISOM_WRITE
	if (!movie || movie->openMode != GF_ISOM_OPEN_WRITE || !movie->editFileMap) //memory mode
		return GF_NOT_SUPPORTED;

	if (movie->segment_bs)
		*out_bs = movie->segment_bs;
	else
		*out_bs = movie->editFileMap->bs;

	if (movie->moof)
		movie->moof->fragment_offset = 0;

	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}
#endif


GF_EXPORT
GF_Err gf_isom_write(GF_ISOFile *movie)
{
	GF_Err e;
	if (movie == NULL) return GF_ISOM_INVALID_FILE;
	e = GF_OK;

	//update duration of each track
	if (movie->moov) {
		u32 i, count = gf_list_count(movie->moov->trackList);
		for (i=0; i<count; i++) {
			GF_TrackBox *trak = gf_list_get(movie->moov->trackList, i);
			e = SetTrackDuration(trak);
			if (e) return e;
		}
	}

#ifndef GPAC_DISABLE_ISOM_WRITE
	//write our movie to the file
	if ((movie->openMode != GF_ISOM_OPEN_READ) && (movie->openMode != GF_ISOM_OPEN_READ_EDIT)) {
		gf_isom_get_duration(movie);
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		//movie fragment mode, just store the fragment
		if ( (movie->openMode == GF_ISOM_OPEN_WRITE) && (movie->FragmentsFlags & GF_ISOM_FRAG_WRITE_READY) ) {
			e = gf_isom_close_fragments(movie);
			if (e) return e;
			//in case of mfra box usage -> create mfro, calculate box sizes and write it out
			if (movie->mfra) {
				if (!movie->mfra->mfro) {
					movie->mfra->mfro = (GF_MovieFragmentRandomAccessOffsetBox *)gf_isom_box_new_parent(&movie->mfra->child_boxes, GF_ISOM_BOX_TYPE_MFRO);
					if (!movie->mfra->mfro) return GF_OUT_OF_MEM;
				}
				e = gf_isom_box_size((GF_Box *)movie->mfra);
				if (e) return e;
				movie->mfra->mfro->container_size = (u32) movie->mfra->size;

				//write mfra
				if (!strcmp(movie->fileName, "_gpac_isobmff_redirect") && movie->on_block_out) {
					GF_BitStream *bs = gf_bs_new_cbk(movie->on_block_out, movie->on_block_out_usr_data, movie->on_block_out_block_size);

					e = gf_isom_box_write((GF_Box *)movie->mfra, bs);
					gf_bs_del(bs);
				} else {
					e = gf_isom_box_write((GF_Box *)movie->mfra, movie->editFileMap->bs);
				}
			}
		} else
#endif
			e = WriteToFile(movie, GF_FALSE);
	}
#endif /*GPAC_DISABLE_ISOM_WRITE*/

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (movie->moov) {
		u32 i;
		for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
			GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(movie->moov->trackList, i);
			/*delete any pending dataHandler of scalable enhancements*/
			if (trak->Media && trak->Media->information && trak->Media->information->scalableDataHandler && (trak->Media->information->scalableDataHandler != movie->movieFileMap))
				gf_isom_datamap_del(trak->Media->information->scalableDataHandler);
		}
	}
#endif

	return e;
}

GF_EXPORT
GF_Err gf_isom_close(GF_ISOFile *movie)
{
	GF_Err e=GF_OK;
	if (movie == NULL) return GF_ISOM_INVALID_FILE;
	e = gf_isom_write(movie);
	//free and return;
	gf_isom_delete_movie(movie);
	return e;
}


#if 0 //unused
/*! checks if files has root OD/IOD or not
\param isom_file the target ISO file
\return GF_TRUE if the file has a root OD or IOD */
Bool gf_isom_has_root_od(GF_ISOFile *movie)
{
	if (!movie || !movie->moov || !movie->moov->iods || !movie->moov->iods->descriptor) return GF_FALSE;
	return GF_TRUE;
}
#endif

GF_EXPORT
void gf_isom_disable_odf_conversion(GF_ISOFile *movie, Bool disable)
{
	if (movie) movie->disable_odf_translate = disable ? 1 : 0;
}

//this funct is used for exchange files, where the iods contains an OD
GF_EXPORT
GF_Descriptor *gf_isom_get_root_od(GF_ISOFile *movie)
{
	GF_Descriptor *desc;
	GF_ObjectDescriptor *od;
	GF_InitialObjectDescriptor *iod;
	GF_IsomObjectDescriptor *isom_od;
	GF_IsomInitialObjectDescriptor *isom_iod;
	GF_ESD *esd;
	GF_ES_ID_Inc *inc;
	u32 i;
	u8 useIOD;

	if (!movie || !movie->moov) return NULL;
	if (!movie->moov->iods) return NULL;

	if (movie->disable_odf_translate) {
		//duplicate our descriptor
		movie->LastError = gf_odf_desc_copy((GF_Descriptor *) movie->moov->iods->descriptor, &desc);
		if (movie->LastError) return NULL;
		return desc;
	}
	od = NULL;
	iod = NULL;

	switch (movie->moov->iods->descriptor->tag) {
	case GF_ODF_ISOM_OD_TAG:
		od = (GF_ObjectDescriptor*)gf_malloc(sizeof(GF_ObjectDescriptor));
		if (!od) return NULL;

		memset(od, 0, sizeof(GF_ObjectDescriptor));
		od->ESDescriptors = gf_list_new();
		useIOD = 0;
		break;
	case GF_ODF_ISOM_IOD_TAG:
		iod = (GF_InitialObjectDescriptor*)gf_malloc(sizeof(GF_InitialObjectDescriptor));
		if (!iod) return NULL;

		memset(iod, 0, sizeof(GF_InitialObjectDescriptor));
		iod->ESDescriptors = gf_list_new();
		useIOD = 1;
		break;
	default:
		return NULL;
	}

	//duplicate our descriptor
	movie->LastError = gf_odf_desc_copy((GF_Descriptor *) movie->moov->iods->descriptor, &desc);
	if (movie->LastError) {
		if (od) {
			gf_list_del(od->ESDescriptors);
			gf_free(od);
		}
		if (iod) {
			gf_list_del(iod->ESDescriptors);
			gf_free(iod);
		}
		return NULL;
	}

	if (!useIOD) {
		isom_od = (GF_IsomObjectDescriptor *)desc;
		od->objectDescriptorID = isom_od->objectDescriptorID;
		od->extensionDescriptors = isom_od->extensionDescriptors;
		isom_od->extensionDescriptors = NULL;
		od->IPMP_Descriptors = isom_od->IPMP_Descriptors;
		isom_od->IPMP_Descriptors = NULL;
		od->OCIDescriptors = isom_od->OCIDescriptors;
		isom_od->OCIDescriptors = NULL;
		od->URLString = isom_od->URLString;
		isom_od->URLString = NULL;
		od->tag = GF_ODF_OD_TAG;
		//then recreate the desc in Inc
		i=0;
		while ((inc = (GF_ES_ID_Inc*)gf_list_enum(isom_od->ES_ID_IncDescriptors, &i))) {
			movie->LastError = GetESDForTime(movie->moov, inc->trackID, 0, &esd);
			if (!movie->LastError) movie->LastError = gf_list_add(od->ESDescriptors, esd);
			if (movie->LastError) {
				gf_odf_desc_del(desc);
				gf_odf_desc_del((GF_Descriptor *) od);
				return NULL;
			}
		}
		gf_odf_desc_del(desc);
		return (GF_Descriptor *)od;
	} else {
		isom_iod = (GF_IsomInitialObjectDescriptor *)desc;
		iod->objectDescriptorID = isom_iod->objectDescriptorID;
		iod->extensionDescriptors = isom_iod->extensionDescriptors;
		isom_iod->extensionDescriptors = NULL;
		iod->IPMP_Descriptors = isom_iod->IPMP_Descriptors;
		isom_iod->IPMP_Descriptors = NULL;
		iod->OCIDescriptors = isom_iod->OCIDescriptors;
		isom_iod->OCIDescriptors = NULL;
		iod->URLString = isom_iod->URLString;
		isom_iod->URLString = NULL;
		iod->tag = GF_ODF_IOD_TAG;

		iod->audio_profileAndLevel = isom_iod->audio_profileAndLevel;
		iod->graphics_profileAndLevel = isom_iod->graphics_profileAndLevel;
		iod->inlineProfileFlag = isom_iod->inlineProfileFlag;
		iod->OD_profileAndLevel = isom_iod->OD_profileAndLevel;
		iod->scene_profileAndLevel = isom_iod->scene_profileAndLevel;
		iod->visual_profileAndLevel = isom_iod->visual_profileAndLevel;
		iod->IPMPToolList = isom_iod->IPMPToolList;
		isom_iod->IPMPToolList = NULL;

		//then recreate the desc in Inc
		i=0;
		while ((inc = (GF_ES_ID_Inc*)gf_list_enum(isom_iod->ES_ID_IncDescriptors, &i))) {
			movie->LastError = GetESDForTime(movie->moov, inc->trackID, 0, &esd);
			if (!movie->LastError) movie->LastError = gf_list_add(iod->ESDescriptors, esd);
			if (movie->LastError) {
				gf_odf_desc_del(desc);
				gf_odf_desc_del((GF_Descriptor *) iod);
				return NULL;
			}
		}
		gf_odf_desc_del(desc);
		return (GF_Descriptor *)iod;
	}
}


GF_EXPORT
u32 gf_isom_get_track_count(GF_ISOFile *movie)
{
	if (!movie || !movie->moov) return 0;

	if (!movie->moov->trackList) {
		movie->LastError = GF_ISOM_INVALID_FILE;
		return 0;
	}
	return gf_list_count(movie->moov->trackList);
}


GF_EXPORT
GF_ISOTrackID gf_isom_get_track_id(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	if (!movie) return 0;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Header) return 0;
	return trak->Header->trackID;
}


GF_EXPORT
u32 gf_isom_get_track_by_id(GF_ISOFile *the_file, GF_ISOTrackID trackID)
{
	u32 count;
	u32 i;
	if (the_file == NULL) return 0;

	count = gf_isom_get_track_count(the_file);
	if (!count) return 0;
	for (i = 0; i < count; i++) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, i+1);
		if (!trak || !trak->Header) return 0;
		if (trak->Header->trackID == trackID) return i+1;
	}
	return 0;
}

GF_EXPORT
GF_ISOTrackID gf_isom_get_track_original_id(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	if (!movie) return 0;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return 0;
	return trak->originalID;
}

//return the timescale of the movie, 0 if error
GF_EXPORT
Bool gf_isom_has_movie(GF_ISOFile *file)
{
	if (file && file->moov) return GF_TRUE;
	return GF_FALSE;
}

#ifndef GPAC_DISABLE_ISOM
GF_EXPORT
Bool gf_isom_has_segment(GF_ISOFile *file, u32 *brand, u32 *version)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	u32 i;
	GF_Box *a;
	i = 0;
	while (NULL != (a = (GF_Box*)gf_list_enum(file->TopBoxes, &i))) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
		if (a->type == GF_ISOM_BOX_TYPE_STYP) {
			GF_FileTypeBox *styp = (GF_FileTypeBox *)a;
			*brand = styp->majorBrand;
			*version = styp->minorVersion;
			return GF_TRUE;
		}
#endif
	}
#endif
	return GF_FALSE;
}

GF_EXPORT
u32 gf_isom_segment_get_fragment_count(GF_ISOFile *file)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (file) {
		u32 i, count = 0;
		for (i=0; i<gf_list_count(file->TopBoxes); i++) {
			GF_Box *a = (GF_Box*)gf_list_get(file->TopBoxes, i);
			if (a->type==GF_ISOM_BOX_TYPE_MOOF) count++;
		}
		return count;
	}
#endif
	return 0;
}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
static GF_MovieFragmentBox *gf_isom_get_moof(GF_ISOFile *file, u32 moof_index)
{
	u32 i;
	for (i=0; i<gf_list_count(file->TopBoxes); i++) {
		GF_Box *a = (GF_Box*)gf_list_get(file->TopBoxes, i);
		if (a->type==GF_ISOM_BOX_TYPE_MOOF) {
			moof_index--;
			if (!moof_index) return (GF_MovieFragmentBox *) a;
		}
	}
	return NULL;
}
#endif /* GPAC_DISABLE_ISOM_FRAGMENTS */

GF_EXPORT
u32 gf_isom_segment_get_track_fragment_count(GF_ISOFile *file, u32 moof_index)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	GF_MovieFragmentBox *moof;
	if (!file) return 0;
	gf_list_count(file->TopBoxes);
	moof = gf_isom_get_moof(file, moof_index);
	return moof ? gf_list_count(moof->TrackList) : 0;
#endif
	return 0;
}

GF_EXPORT
u32 gf_isom_segment_get_track_fragment_decode_time(GF_ISOFile *file, u32 moof_index, u32 traf_index, u64 *decode_time)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	GF_MovieFragmentBox *moof;
	GF_TrackFragmentBox *traf;
	if (!file) return 0;
	gf_list_count(file->TopBoxes);
	moof = gf_isom_get_moof(file, moof_index);
	traf = moof ? (GF_TrackFragmentBox*)gf_list_get(moof->TrackList, traf_index-1) : NULL;
	if (!traf) return 0;
	if (decode_time) {
		*decode_time = traf->tfdt ? traf->tfdt->baseMediaDecodeTime : 0;
	}
	return traf->tfhd->trackID;
#endif
	return 0;
}

GF_EXPORT
u64 gf_isom_segment_get_fragment_size(GF_ISOFile *file, u32 moof_index, u32 *moof_size)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (!file) return 0;
	u32 moof_sn=0;
	Bool moof_after_mdat = GF_FALSE;
	u64 size=0;
	u32 i, count = gf_list_count(file->TopBoxes);
	if (moof_size) *moof_size = 0;
	for (i=0; i<count; i++) {
		GF_Box *b = gf_list_get(file->TopBoxes, i);
		size += b->size;
		if (b->type == GF_ISOM_BOX_TYPE_MOOF) {
			if (!moof_after_mdat && (moof_sn == moof_index))
				return size - b->size;

			if (moof_size) *moof_size = (u32) b->size;
			moof_sn++;
			if (moof_after_mdat && (moof_sn == moof_index))
				return size;
			if (moof_after_mdat) size=0;
			if ((moof_sn>1) && !moof_after_mdat) size = b->size;
		}
		if (b->type == GF_ISOM_BOX_TYPE_MDAT) {
			if (!moof_sn) moof_after_mdat = GF_TRUE;
		}
	}
	return size;
#endif
	return 0;
}
#endif

//return the timescale of the movie, 0 if error
GF_EXPORT
u32 gf_isom_get_timescale(GF_ISOFile *movie)
{
	if (!movie || !movie->moov || !movie->moov->mvhd) return 0;
	return movie->moov->mvhd->timeScale;
}


//return the duration of the movie, 0 if error
GF_EXPORT
u64 gf_isom_get_duration(GF_ISOFile *movie)
{
	if (!movie || !movie->moov || !movie->moov->mvhd) return 0;

	//if file was open in Write or Edit mode, recompute the duration
	//the duration of a movie is the MaxDuration of all the tracks...

#ifndef GPAC_DISABLE_ISOM_WRITE
	gf_isom_update_duration(movie);
#endif /*GPAC_DISABLE_ISOM_WRITE*/

	return movie->moov->mvhd->duration;
}
//return the duration of the movie, 0 if error
GF_EXPORT
u64 gf_isom_get_original_duration(GF_ISOFile *movie)
{
	if (!movie || !movie->moov|| !movie->moov->mvhd) return 0;
	return movie->moov->mvhd->original_duration;
}

//return the creation info of the movie
GF_EXPORT
GF_Err gf_isom_get_creation_time(GF_ISOFile *movie, u64 *creationTime, u64 *modificationTime)
{
	if (!movie || !movie->moov) return GF_BAD_PARAM;

	if (creationTime) *creationTime = movie->moov->mvhd->creationTime;
	if (creationTime) *modificationTime = movie->moov->mvhd->modificationTime;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_track_creation_time(GF_ISOFile *movie, u32 trackNumber, u64 *creationTime, u64 *modificationTime)
{
	GF_TrackBox *trak;
	if (!movie || !movie->moov) return GF_BAD_PARAM;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return 0;

	if (creationTime) *creationTime = trak->Media->mediaHeader->creationTime;
	if (creationTime) *modificationTime = trak->Media->mediaHeader->modificationTime;
	return GF_OK;
}

//check the presence of a track in IOD. 0: NO, 1: YES, 2: ERROR
GF_EXPORT
u8 gf_isom_is_track_in_root_od(GF_ISOFile *movie, u32 trackNumber)
{
	u32 i;
	GF_ISOTrackID trackID;
	GF_Descriptor *desc;
	GF_ES_ID_Inc *inc;
	GF_List *inc_list;
	if (!movie) return 2;
	if (!movie->moov || !movie->moov->iods) return 0;

	desc = movie->moov->iods->descriptor;
	switch (desc->tag) {
	case GF_ODF_ISOM_IOD_TAG:
		inc_list = ((GF_IsomInitialObjectDescriptor *)desc)->ES_ID_IncDescriptors;
		break;
	case GF_ODF_ISOM_OD_TAG:
		inc_list = ((GF_IsomObjectDescriptor *)desc)->ES_ID_IncDescriptors;
		break;
	//files without IOD are possible !
	default:
		return 0;
	}
	trackID = gf_isom_get_track_id(movie, trackNumber);
	if (!trackID) return 2;
	i=0;
	while ((inc = (GF_ES_ID_Inc*)gf_list_enum(inc_list, &i))) {
		if (inc->trackID == (u32) trackID) return 1;
	}
	return 0;
}



//gets the enable flag of a track
//0: NO, 1: YES, 2: ERROR
GF_EXPORT
u8 gf_isom_is_track_enabled(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);

	if (!trak || !trak->Header) return 2;
	return (trak->Header->flags & 1) ? 1 : 0;
}

GF_EXPORT
u32 gf_isom_get_track_flags(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;
	return trak->Header->flags;
}


//get the track duration
//return 0 if bad param
GF_EXPORT
u64 gf_isom_get_track_duration(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return 0;

#ifndef GPAC_DISABLE_ISOM_WRITE
	/*in all modes except dump recompute duration in case headers are wrong*/
	if (movie->openMode != GF_ISOM_OPEN_READ_DUMP) {
		SetTrackDuration(trak);
	}
#endif
	return trak->Header->duration;
}

GF_EXPORT
GF_Err gf_isom_get_media_language(GF_ISOFile *the_file, u32 trackNumber, char **lang)
{
	u32 count;
	Bool elng_found = GF_FALSE;
	GF_TrackBox *trak;
	if (!lang) {
		return GF_BAD_PARAM;
	}
	*lang = NULL;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;
	count = gf_list_count(trak->Media->child_boxes);
	if (count>0) {
		u32 i;
		for (i = 0; i < count; i++) {
			GF_Box *box = (GF_Box *)gf_list_get(trak->Media->child_boxes, i);
			if (box->type == GF_ISOM_BOX_TYPE_ELNG) {
				*lang = gf_strdup(((GF_ExtendedLanguageBox *)box)->extended_language);
				return GF_OK;
			}
		}
	}
	if (!elng_found) {
		*lang = gf_strdup(trak->Media->mediaHeader->packedLanguage);
	}
	return GF_OK;
}

GF_EXPORT
u32 gf_isom_get_track_kind_count(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_UserDataBox *udta;
	GF_UserDataMap *map;
	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
		if (!trak) return 0;
		if (!trak->udta) {
			return 0;
		}
		udta = trak->udta;
	} else {
		return 0;
	}
	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_KIND, NULL);
	if (!map) return 0;

	return gf_list_count(map->boxes);
}

GF_EXPORT
GF_Err gf_isom_get_track_kind(GF_ISOFile *the_file, u32 trackNumber, u32 index, char **scheme, char **value)
{
	GF_Err e;
	GF_UserDataBox *udta;
	GF_UserDataMap *map;
	GF_KindBox *kindBox;
	if (!scheme || !value) {
		return GF_BAD_PARAM;
	}
	*scheme = NULL;
	*value = NULL;

	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		if (!trak->udta) {
			e = trak_on_child_box((GF_Box*)trak, gf_isom_box_new_parent(&trak->child_boxes, GF_ISOM_BOX_TYPE_UDTA), GF_FALSE);
			if (e) return e;
		}
		udta = trak->udta;
	} else {
		return GF_BAD_PARAM;
	}
	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_KIND, NULL);
	if (!map) return GF_BAD_PARAM;

	kindBox = (GF_KindBox *)gf_list_get(map->boxes, index);
	if (!kindBox) return GF_BAD_PARAM;

	*scheme = gf_strdup(kindBox->schemeURI);
	if (kindBox->value) {
		*value = gf_strdup(kindBox->value);
	}
	return GF_OK;
}


//Return the number of track references of a track for a given ReferenceType
//return 0 if error
GF_EXPORT
s32 gf_isom_get_reference_count(GF_ISOFile *movie, u32 trackNumber, u32 referenceType)
{
	GF_TrackBox *trak;
	GF_TrackReferenceTypeBox *dpnd;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return -1;
	if (!trak->References) return 0;
	if (movie->openMode == GF_ISOM_OPEN_WRITE) {
		movie->LastError = GF_ISOM_INVALID_MODE;
		return -1;
	}

	dpnd = NULL;
	if ( (movie->LastError = Track_FindRef(trak, referenceType, &dpnd)) ) return -1;
	if (!dpnd) return 0;
	return dpnd->trackIDCount;
}


//Return the number of track references of a track for a given ReferenceType
//return 0 if error
GF_EXPORT
const GF_ISOTrackID *gf_isom_enum_track_references(GF_ISOFile *movie, u32 trackNumber, u32 idx, u32 *referenceType, u32 *referenceCount)
{
	GF_TrackBox *trak;
	GF_TrackReferenceTypeBox *dpnd;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return NULL;
	if (!trak->References) return NULL;
	dpnd = gf_list_get(trak->References->child_boxes, idx);
	if (!dpnd) return NULL;
	*referenceType = dpnd->reference_type;
	*referenceCount = dpnd->trackIDCount;
	return dpnd->trackIDs;
}


//Return the referenced track number for a track and a given ReferenceType and Index
//return -1 if error, 0 if the reference is a NULL one, or the trackNumber
GF_EXPORT
GF_Err gf_isom_get_reference(GF_ISOFile *movie, u32 trackNumber, u32 referenceType, u32 referenceIndex, u32 *refTrack)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_TrackReferenceTypeBox *dpnd;
	GF_ISOTrackID refTrackNum;
	trak = gf_isom_get_track_from_file(movie, trackNumber);

	*refTrack = 0;
	if (!trak || !trak->References) return GF_BAD_PARAM;

	dpnd = NULL;
	e = Track_FindRef(trak, referenceType, &dpnd);
	if (e) return e;
	if (!dpnd) return GF_BAD_PARAM;

	if (!referenceIndex || (referenceIndex > dpnd->trackIDCount)) return GF_BAD_PARAM;

	//the spec allows a NULL reference
	//(ex, to force desync of a track, set a sync ref with ID = 0)
	if (dpnd->trackIDs[referenceIndex - 1] == 0) return GF_OK;

	refTrackNum = gf_isom_get_tracknum_from_id(movie->moov, dpnd->trackIDs[referenceIndex-1]);

	//if the track was not found, this means the file is broken !!!
	if (! refTrackNum) return GF_ISOM_INVALID_FILE;
	*refTrack = refTrackNum;
	return GF_OK;
}

//Return the referenced track ID for a track and a given ReferenceType and Index
//return -1 if error, 0 if the reference is a NULL one, or the trackNumber
GF_EXPORT
GF_Err gf_isom_get_reference_ID(GF_ISOFile *movie, u32 trackNumber, u32 referenceType, u32 referenceIndex, GF_ISOTrackID *refTrackID)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_TrackReferenceTypeBox *dpnd;
	trak = gf_isom_get_track_from_file(movie, trackNumber);

	*refTrackID = 0;
	if (!trak || !trak->References || !referenceIndex) return GF_BAD_PARAM;

	dpnd = NULL;
	e = Track_FindRef(trak, referenceType, &dpnd);
	if (e) return e;
	if (!dpnd) return GF_BAD_PARAM;

	if (referenceIndex > dpnd->trackIDCount) return GF_BAD_PARAM;

	*refTrackID = dpnd->trackIDs[referenceIndex-1];

	return GF_OK;
}

//Return referenceIndex if the given track has a reference to the given TreckID of a given ReferenceType
//return 0 if error
GF_EXPORT
u32 gf_isom_has_track_reference(GF_ISOFile *movie, u32 trackNumber, u32 referenceType, GF_ISOTrackID refTrackID)
{
	u32 i;
	GF_TrackBox *trak;
	GF_TrackReferenceTypeBox *dpnd;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return 0;
	if (!trak->References) return 0;

	dpnd = NULL;
	if ( (movie->LastError = Track_FindRef(trak, referenceType, &dpnd)) ) return 0;
	if (!dpnd) return 0;
	for (i=0; i<dpnd->trackIDCount; i++) {
		if (dpnd->trackIDs[i]==refTrackID) return i+1;
	}
	return 0;
}



//Return the media time given the absolute time in the Movie
GF_EXPORT
GF_Err gf_isom_get_media_time(GF_ISOFile *the_file, u32 trackNumber, u32 movieTime, u64 *MediaTime)
{
	GF_TrackBox *trak;
	u8 useEdit;
	s64 SegmentStartTime, mediaOffset;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !MediaTime) return GF_BAD_PARAM;

	SegmentStartTime = 0;
	return GetMediaTime(trak, GF_FALSE, movieTime, MediaTime, &SegmentStartTime, &mediaOffset, &useEdit, NULL);
}


//Get the stream description index (eg, the ESD) for a given time IN MEDIA TIMESCALE
//return 0 if error or if empty
GF_EXPORT
u32 gf_isom_get_sample_description_index(GF_ISOFile *movie, u32 trackNumber, u64 for_time)
{
	u32 streamDescIndex;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return 0;

	if ( (movie->LastError = Media_GetSampleDescIndex(trak->Media, for_time, &streamDescIndex)) ) {
		return 0;
	}
	return streamDescIndex;
}

//Get the number of "streams" stored in the media - a media can have several stream descriptions...
GF_EXPORT
u32 gf_isom_get_sample_description_count(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	return gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
}


//Get the GF_ESD given the StreamDescriptionIndex
//THE DESCRIPTOR IS DUPLICATED, SO HAS TO BE DELETED BY THE APP
GF_EXPORT
GF_ESD *gf_isom_get_esd(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_ESD *esd;
	GF_Err e;
	e = GetESD(movie->moov, gf_isom_get_track_id(movie, trackNumber), StreamDescriptionIndex, &esd);
	if (e && (e!= GF_ISOM_INVALID_MEDIA)) {
		movie->LastError = e;
		if (esd) gf_odf_desc_del((GF_Descriptor *)esd);
		return NULL;
	}

	return esd;
}

//Get the decoderConfigDescriptor given the SampleDescriptionIndex
//THE DESCRIPTOR IS DUPLICATED, SO HAS TO BE DELETED BY THE APP
GF_EXPORT
GF_DecoderConfig *gf_isom_get_decoder_config(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_ESD *esd;
	GF_Descriptor *decInfo;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return NULL;
	//get the ESD (possibly emulated)
	Media_GetESD(trak->Media, StreamDescriptionIndex, &esd, GF_FALSE);
	if (!esd) return NULL;
	decInfo = (GF_Descriptor *) esd->decoderConfig;
	esd->decoderConfig = NULL;
	gf_odf_desc_del((GF_Descriptor *) esd);
	return (GF_DecoderConfig *)decInfo;
}


//get the media duration (without edit)
//return 0 if bad param
GF_EXPORT
u64 gf_isom_get_media_duration(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return 0;


#ifndef GPAC_DISABLE_ISOM_WRITE

	/*except in dump mode always recompute the duration*/
	if (movie->openMode != GF_ISOM_OPEN_READ_DUMP) {
		if ( (movie->LastError = Media_SetDuration(trak)) ) return 0;
	}

#endif

	return trak->Media->mediaHeader->duration;
}

//get the media duration (without edit)
//return 0 if bad param
GF_EXPORT
u64 gf_isom_get_media_original_duration(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !trak->Media->mediaHeader) return 0;

	return trak->Media->mediaHeader->original_duration;
}

//Get the timeScale of the media. All samples DTS/CTS are expressed in this timeScale
GF_EXPORT
u32 gf_isom_get_media_timescale(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->mediaHeader) return 0;
	return trak->Media->mediaHeader->timeScale;
}


GF_EXPORT
u32 gf_isom_get_copyright_count(GF_ISOFile *mov)
{
	GF_UserDataMap *map;
	if (!mov || !mov->moov || !mov->moov->udta) return 0;
	map = udta_getEntry(mov->moov->udta, GF_ISOM_BOX_TYPE_CPRT, NULL);
	if (!map) return 0;
	return gf_list_count(map->boxes);
}

GF_EXPORT
GF_Err gf_isom_get_copyright(GF_ISOFile *mov, u32 Index, const char **threeCharCode, const char **notice)
{
	GF_UserDataMap *map;
	GF_CopyrightBox *cprt;

	if (!mov || !mov->moov || !Index) return GF_BAD_PARAM;

	if (!mov->moov->udta) return GF_OK;
	map = udta_getEntry(mov->moov->udta, GF_ISOM_BOX_TYPE_CPRT, NULL);
	if (!map) return GF_OK;

	if (Index > gf_list_count(map->boxes)) return GF_BAD_PARAM;

	cprt = (GF_CopyrightBox*)gf_list_get(map->boxes, Index-1);
	(*threeCharCode) = cprt->packedLanguageCode;
	(*notice) = cprt->notice;
	return GF_OK;
}

#if 0
GF_Err gf_isom_get_watermark(GF_ISOFile *mov, bin128 UUID, u8** data, u32* length)
{
	GF_UserDataMap *map;
	GF_UnknownUUIDBox *wm;

	if (!mov) return GF_BAD_PARAM;
	if (!mov->moov || !mov->moov->udta) return GF_NOT_SUPPORTED;

	map = udta_getEntry(mov->moov->udta, GF_ISOM_BOX_TYPE_UUID, (bin128 *) & UUID);
	if (!map) return GF_NOT_SUPPORTED;

	wm = (GF_UnknownUUIDBox*)gf_list_get(map->boxes, 0);
	if (!wm) return GF_NOT_SUPPORTED;

	*data = (u8 *) gf_malloc(sizeof(char)*wm->dataSize);
	if (! *data) return GF_OUT_OF_MEM;
	memcpy(*data, wm->data, wm->dataSize);
	*length = wm->dataSize;
	return GF_OK;
}
#endif

GF_EXPORT
u32 gf_isom_get_chapter_count(GF_ISOFile *movie, u32 trackNumber)
{
	GF_UserDataMap *map;
	GF_ChapterListBox *lst;
	GF_UserDataBox *udta;

	if (!movie || !movie->moov) return 0;

	udta = NULL;
	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return 0;
		udta = trak->udta;
	} else {
		udta = movie->moov->udta;
	}
	if (!udta) return 0;
	map = udta_getEntry(udta, GF_ISOM_BOX_TYPE_CHPL, NULL);
	if (!map) return 0;
	lst = (GF_ChapterListBox *)gf_list_get(map->boxes, 0);
	if (!lst) return 0;
	return gf_list_count(lst->list);
}

GF_EXPORT
GF_Err gf_isom_get_chapter(GF_ISOFile *movie, u32 trackNumber, u32 Index, u64 *chapter_time, const char **name)
{
	GF_UserDataMap *map;
	GF_ChapterListBox *lst;
	GF_ChapterEntry *ce;
	GF_UserDataBox *udta;

	if (!movie || !movie->moov) return GF_BAD_PARAM;

	udta = NULL;
	if (trackNumber) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		udta = trak->udta;
	} else {
		udta = movie->moov->udta;
	}
	if (!udta) return GF_BAD_PARAM;
	map = udta_getEntry(movie->moov->udta, GF_ISOM_BOX_TYPE_CHPL, NULL);
	if (!map) return GF_BAD_PARAM;
	lst = (GF_ChapterListBox *)gf_list_get(map->boxes, 0);
	if (!lst) return GF_BAD_PARAM;

	ce = (GF_ChapterEntry *)gf_list_get(lst->list, Index-1);
	if (!ce) return GF_BAD_PARAM;
	if (chapter_time) {
		*chapter_time = ce->start_time;
		*chapter_time /= 10000L;
	}
	if (name) *name = ce->name;
	return GF_OK;
}


GF_EXPORT
u32 gf_isom_get_media_type(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	return (trak->Media && trak->Media->handler) ? trak->Media->handler->handlerType : 0;
}

Bool IsMP4Description(u32 entryType)
{
	switch (entryType) {
	case GF_ISOM_BOX_TYPE_MP4S:
	case GF_ISOM_BOX_TYPE_LSR1:
	case GF_ISOM_BOX_TYPE_MP4A:
	case GF_ISOM_BOX_TYPE_MP4V:
	case GF_ISOM_BOX_TYPE_ENCA:
	case GF_ISOM_BOX_TYPE_ENCV:
	case GF_ISOM_BOX_TYPE_RESV:
	case GF_ISOM_BOX_TYPE_ENCS:
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}

Bool gf_isom_is_encrypted_entry(u32 entryType)
{
	switch (entryType) {
	case GF_ISOM_BOX_TYPE_ENCA:
	case GF_ISOM_BOX_TYPE_ENCV:
	case GF_ISOM_BOX_TYPE_ENCS:
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}

GF_EXPORT
Bool gf_isom_is_track_encrypted(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	u32 i=0;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 2;
	while (1) {
		GF_Box *entry = (GF_Box*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, i);
		if (!entry) break;
		if (gf_isom_is_encrypted_entry(entry->type)) return GF_TRUE;

		if (gf_isom_is_cenc_media(the_file, trackNumber, i+1))
			return GF_TRUE;

		i++;
	}
	return GF_FALSE;
}

GF_EXPORT
u32 gf_isom_get_media_subtype(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Box *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !DescriptionIndex || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable) return 0;
	entry = (GF_Box*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	if (!entry) return 0;

	//filter MPEG sub-types
	if (IsMP4Description(entry->type)) {
		if (gf_isom_is_encrypted_entry(entry->type)) return GF_ISOM_SUBTYPE_MPEG4_CRYP;
		else return GF_ISOM_SUBTYPE_MPEG4;
	}
	if (entry->type == GF_ISOM_BOX_TYPE_GNRV) {
		return ((GF_GenericVisualSampleEntryBox *)entry)->EntryType;
	}
	else if (entry->type == GF_ISOM_BOX_TYPE_GNRA) {
		return ((GF_GenericAudioSampleEntryBox *)entry)->EntryType;
	}
	else if (entry->type == GF_ISOM_BOX_TYPE_GNRM) {
		return ((GF_GenericSampleEntryBox *)entry)->EntryType;
	}
	return entry->type;
}

GF_EXPORT
u32 gf_isom_get_mpeg4_subtype(GF_ISOFile *the_file, u32 trackNumber, u32 DescriptionIndex)
{
	GF_TrackBox *trak;
	GF_Box *entry=NULL;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !DescriptionIndex) return 0;

	if (trak->Media
		&& trak->Media->information
		&& trak->Media->information->sampleTable
		&& trak->Media->information->sampleTable->SampleDescription
	) {
		entry = (GF_Box*)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, DescriptionIndex-1);
	}
	if (!entry) return 0;

	//filter MPEG sub-types
	if (!IsMP4Description(entry->type)) return 0;
	return entry->type;
}

//Get the HandlerDescription name.
GF_EXPORT
GF_Err gf_isom_get_handler_name(GF_ISOFile *the_file, u32 trackNumber, const char **outName)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !outName) return GF_BAD_PARAM;
	*outName = trak->Media->handler->nameUTF8;
	return GF_OK;
}

//Check the DataReferences of this track
GF_EXPORT
GF_Err gf_isom_check_data_reference(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_Err e;
	u32 drefIndex;
	GF_TrackBox *trak;

	if (!StreamDescriptionIndex || !trackNumber) return GF_BAD_PARAM;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, StreamDescriptionIndex , NULL, &drefIndex);
	if (e) return e;
	if (!drefIndex) return GF_BAD_PARAM;
	return Media_CheckDataEntry(trak->Media, drefIndex);
}

//get the location of the data. If URL && URN are NULL, the data is in this file
GF_EXPORT
GF_Err gf_isom_get_data_reference(GF_ISOFile *the_file, u32 trackNumber, u32 StreamDescriptionIndex, const char **outURL, const char **outURN)
{
	GF_TrackBox *trak;
	GF_DataEntryURLBox *url=NULL;
	GF_DataEntryURNBox *urn;
	u32 drefIndex;
	GF_Err e;

	*outURL = *outURN = NULL;

	if (!StreamDescriptionIndex || !trackNumber) return GF_BAD_PARAM;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	e = Media_GetSampleDesc(trak->Media, StreamDescriptionIndex , NULL, &drefIndex);
	if (e) return e;
	if (!drefIndex) return GF_BAD_PARAM;

	if (trak->Media
		&& trak->Media->information
		&& trak->Media->information->dataInformation
		&& trak->Media->information->dataInformation->dref
	) {
		url = (GF_DataEntryURLBox*)gf_list_get(trak->Media->information->dataInformation->dref->child_boxes, drefIndex - 1);
	}
	if (!url) return GF_ISOM_INVALID_FILE;

	if (url->type == GF_ISOM_BOX_TYPE_URL) {
		*outURL = url->location;
		*outURN = NULL;
	} else if (url->type == GF_ISOM_BOX_TYPE_URN) {
		urn = (GF_DataEntryURNBox *) url;
		*outURN = urn->nameURN;
		*outURL = urn->location;
	} else {
		*outURN = NULL;
		*outURL = NULL;
	}
	return GF_OK;
}

//Get the number of samples
//return 0 if error or empty
GF_EXPORT
u32 gf_isom_get_sample_count(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->SampleSize) return 0;
	return trak->Media->information->sampleTable->SampleSize->sampleCount
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	       + trak->sample_count_at_seg_start
#endif
	       ;
}

GF_EXPORT
u32 gf_isom_get_constant_sample_size(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->SampleSize) return 0;
	return trak->Media->information->sampleTable->SampleSize->sampleSize;
}

GF_EXPORT
u32 gf_isom_get_constant_sample_duration(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->TimeToSample) return 0;
	if (trak->Media->information->sampleTable->TimeToSample->nb_entries != 1) return 0;
	return trak->Media->information->sampleTable->TimeToSample->entries[0].sampleDelta;
}

GF_EXPORT
Bool gf_isom_enable_raw_pack(GF_ISOFile *the_file, u32 trackNumber, u32 pack_num_samples)
{
	u32 afmt, bps, nb_ch;
	Bool from_qt=GF_FALSE;
	GF_TrackBox *trak;
	GF_MPEGAudioSampleEntryBox *entry;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;
	trak->pack_num_samples = 0;
	//we only activate sample packing for raw audio
	if (!trak->Media || !trak->Media->handler) return GF_FALSE;
	if (trak->Media->handler->handlerType != GF_ISOM_MEDIA_AUDIO) return GF_FALSE;
	//and sample duration of 1
	if (!trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->TimeToSample) return GF_FALSE;
	if (trak->Media->information->sampleTable->TimeToSample->nb_entries != 1) return GF_FALSE;
	if (!trak->Media->information->sampleTable->TimeToSample->entries) return GF_FALSE;
	if (trak->Media->information->sampleTable->TimeToSample->entries[0].sampleDelta != 1) return GF_FALSE;
	//and sample with constant size
	if (!trak->Media->information->sampleTable->SampleSize || !trak->Media->information->sampleTable->SampleSize->sampleSize) return GF_FALSE;
	trak->pack_num_samples = pack_num_samples;

	if (!pack_num_samples) return GF_FALSE;

	entry = gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, 0);
	if (!entry) return GF_FALSE;

	if (entry->internal_type!=GF_ISOM_SAMPLE_ENTRY_AUDIO) return GF_FALSE;

	//sanity check, some files have wrong stsz sampleSize for raw audio !
	afmt = gf_audio_fmt_from_isobmf(entry->type);
	bps = gf_audio_fmt_bit_depth(afmt) / 8;
	if (!bps) {
		//unknown format, try QTv2
		if (entry->qtff_mode && (entry->internal_type==GF_ISOM_SAMPLE_ENTRY_AUDIO)) {
			bps = entry->extensions[8]<<24 | entry->extensions[9]<<16 | entry->extensions[10]<<8 | entry->extensions[11];
			from_qt = GF_TRUE;
		}
	}
	nb_ch = entry->channel_count;
	if (entry->qtff_mode && (entry->version==2)) {
		//QTFFv2 audio, channel count is 32 bit, after 32bit size of struct and 64 bit samplerate
		//hence start at 12 in extensions
		nb_ch = entry->extensions[11]<<24 | entry->extensions[12]<<16 | entry->extensions[13]<<8 | entry->extensions[14];
	}

	if (bps) {
		u32 res = trak->Media->information->sampleTable->SampleSize->sampleSize % bps;
		if (res) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("ISOBMF: size mismatch for raw audio sample description: constant sample size %d but %d bytes per channel for %s%s!\n", trak->Media->information->sampleTable->SampleSize->sampleSize,
					bps,
					gf_4cc_to_str(entry->type),
					from_qt ? " (as indicated in QT sample description)" : ""
				));
			trak->Media->information->sampleTable->SampleSize->sampleSize = bps * nb_ch;
		}
	}
	return GF_TRUE;
}

Bool gf_isom_has_time_offset_table(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media->information->sampleTable->CompositionOffset) return GF_FALSE;
	return GF_TRUE;
}

GF_EXPORT
u32 gf_isom_has_time_offset(GF_ISOFile *the_file, u32 trackNumber)
{
	u32 i;
	GF_CompositionOffsetBox *ctts;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media->information->sampleTable->CompositionOffset) return 0;

	//return true at the first offset found
	ctts = trak->Media->information->sampleTable->CompositionOffset;
	for (i=0; i<ctts->nb_entries; i++) {
		if (ctts->entries[i].decodingOffset && ctts->entries[i].sampleCount) return ctts->version ? 2 : 1;
	}
	return 0;
}

GF_EXPORT
s64 gf_isom_get_cts_to_dts_shift(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media->information->sampleTable->CompositionToDecode) return 0;
	return trak->Media->information->sampleTable->CompositionToDecode->compositionToDTSShift;
}

GF_EXPORT
Bool gf_isom_has_sync_shadows(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;
	if (!trak->Media->information->sampleTable->ShadowSync) return GF_FALSE;
	if (gf_list_count(trak->Media->information->sampleTable->ShadowSync->entries) ) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
Bool gf_isom_has_sample_dependency(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;
	if (!trak->Media->information->sampleTable->SampleDep) return GF_FALSE;
	return GF_TRUE;
}

GF_EXPORT
GF_Err gf_isom_get_sample_flags(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *isLeading, u32 *dependsOn, u32 *dependedOn, u32 *redundant)
{
	GF_TrackBox *trak;
	*isLeading = 0;
	*dependsOn = 0;
	*dependedOn = 0;
	*redundant = 0;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->SampleDep) return GF_BAD_PARAM;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sampleNumber <= trak->sample_count_at_seg_start)
		return GF_BAD_PARAM;
	sampleNumber -= trak->sample_count_at_seg_start;
#endif

	return stbl_GetSampleDepType(trak->Media->information->sampleTable->SampleDep, sampleNumber, isLeading, dependsOn, dependedOn, redundant);
}

//return a sample give its number, and set the SampleDescIndex of this sample
//this index allows to retrieve the stream description if needed (2 media in 1 track)
//return NULL if error
GF_EXPORT
GF_ISOSample *gf_isom_get_sample_ex(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex, GF_ISOSample *static_sample, u64 *data_offset)
{
	GF_Err e;
	u32 descIndex;
	GF_TrackBox *trak;
	GF_ISOSample *samp;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return NULL;

	if (!sampleNumber) return NULL;
	if (static_sample) {
		samp = static_sample;
		if (static_sample->dataLength && !static_sample->alloc_size)
			static_sample->alloc_size = static_sample->dataLength;
	} else {
		samp = gf_isom_sample_new();
	}
	if (!samp) return NULL;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sampleNumber<=trak->sample_count_at_seg_start)
		return NULL;
	sampleNumber -= trak->sample_count_at_seg_start;
#endif

	e = Media_GetSample(trak->Media, sampleNumber, &samp, &descIndex, GF_FALSE, data_offset);
	if (static_sample && !static_sample->alloc_size)
		static_sample->alloc_size = static_sample->dataLength;

	if (e) {
		gf_isom_set_last_error(the_file, e);
		if (!static_sample) gf_isom_sample_del(&samp);
		return NULL;
	}
	if (sampleDescriptionIndex) *sampleDescriptionIndex = descIndex;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (samp) samp->DTS += trak->dts_at_seg_start;
#endif

	return samp;
}

GF_EXPORT
GF_ISOSample *gf_isom_get_sample(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex)
{
	return gf_isom_get_sample_ex(the_file, trackNumber, sampleNumber, sampleDescriptionIndex, NULL, NULL);
}

GF_EXPORT
u32 gf_isom_get_sample_duration(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber)
{
	u32 dur;
	u64 dts;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !sampleNumber) return 0;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sampleNumber<=trak->sample_count_at_seg_start) return 0;
	sampleNumber -= trak->sample_count_at_seg_start;
#endif

	stbl_GetSampleDTS_and_Duration(trak->Media->information->sampleTable->TimeToSample, sampleNumber, &dts, &dur);
	return dur;
}


GF_EXPORT
u32 gf_isom_get_sample_size(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber)
{
	u32 size = 0;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !sampleNumber) return 0;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sampleNumber<=trak->sample_count_at_seg_start) return 0;
	sampleNumber -= trak->sample_count_at_seg_start;
#endif
	stbl_GetSampleSize(trak->Media->information->sampleTable->SampleSize, sampleNumber, &size);
	return size;
}

GF_EXPORT
u32 gf_isom_get_max_sample_size(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->SampleSize) return 0;

	return trak->Media->information->sampleTable->SampleSize->max_size;
}

GF_EXPORT
u32 gf_isom_get_avg_sample_size(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->SampleSize) return 0;

	if ( trak->Media->information->sampleTable->SampleSize->sampleSize)
		return trak->Media->information->sampleTable->SampleSize->sampleSize;

	if (!trak->Media->information->sampleTable->SampleSize->total_samples) return 0;
	return (u32) (trak->Media->information->sampleTable->SampleSize->total_size / trak->Media->information->sampleTable->SampleSize->total_samples);
}

GF_EXPORT
u32 gf_isom_get_max_sample_delta(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->TimeToSample) return 0;

	return trak->Media->information->sampleTable->TimeToSample->max_ts_delta;
}

GF_EXPORT
u32 gf_isom_get_avg_sample_delta(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->TimeToSample) return 0;

	GF_TimeToSampleBox *stts = trak->Media->information->sampleTable->TimeToSample;
	u32 i, nb_ent = 0, min = 0;
	for (i=0; i<stts->nb_entries; i++) {
		if (!nb_ent || nb_ent < stts->entries[i].sampleCount) {
			min = stts->entries[i].sampleDelta;
			nb_ent = stts->entries[i].sampleCount;
		}
	}
	return min;
}


GF_EXPORT
u32 gf_isom_get_max_sample_cts_offset(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->CompositionOffset) return 0;

	return trak->Media->information->sampleTable->CompositionOffset->max_cts_delta;
}


GF_EXPORT
Bool gf_isom_get_sample_sync(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber)
{
	GF_ISOSAPType is_rap;
	GF_Err e;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !sampleNumber) return GF_FALSE;

	if (! trak->Media->information->sampleTable->SyncSample) return GF_TRUE;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sampleNumber<=trak->sample_count_at_seg_start) return GF_FALSE;
	sampleNumber -= trak->sample_count_at_seg_start;
#endif
	e = stbl_GetSampleRAP(trak->Media->information->sampleTable->SyncSample, sampleNumber, &is_rap, NULL, NULL);
	if (e) return GF_FALSE;
	return is_rap ? GF_TRUE : GF_FALSE;
}

//same as gf_isom_get_sample but doesn't fetch media data
GF_EXPORT
GF_ISOSample *gf_isom_get_sample_info_ex(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex, u64 *data_offset, GF_ISOSample *static_sample)
{
	GF_Err e;
	GF_TrackBox *trak;
	GF_ISOSample *samp;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return NULL;

	if (!sampleNumber) return NULL;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sampleNumber<=trak->sample_count_at_seg_start) return NULL;
	sampleNumber -= trak->sample_count_at_seg_start;
#endif
	if (static_sample) {
		samp = static_sample;
	} else {
		samp = gf_isom_sample_new();
		if (!samp) return NULL;
	}

	e = Media_GetSample(trak->Media, sampleNumber, &samp, sampleDescriptionIndex, GF_TRUE, data_offset);
	if (e) {
		gf_isom_set_last_error(the_file, e);
		if (!static_sample)
			gf_isom_sample_del(&samp);
		return NULL;
	}
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (samp) samp->DTS += trak->dts_at_seg_start;
#endif
	return samp;
}

GF_EXPORT
GF_ISOSample *gf_isom_get_sample_info(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u32 *sampleDescriptionIndex, u64 *data_offset)
{
	return gf_isom_get_sample_info_ex(the_file, trackNumber, sampleNumber, sampleDescriptionIndex, data_offset, NULL);
}


//get sample dts
GF_EXPORT
u64 gf_isom_get_sample_dts(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber)
{
	u64 dts;
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	if (!sampleNumber) return 0;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sampleNumber<=trak->sample_count_at_seg_start) return 0;
	sampleNumber -= trak->sample_count_at_seg_start;
#endif
	if (stbl_GetSampleDTS(trak->Media->information->sampleTable->TimeToSample, sampleNumber, &dts) != GF_OK) return 0;
	return dts;
}

GF_EXPORT
Bool gf_isom_is_self_contained(GF_ISOFile *the_file, u32 trackNumber, u32 sampleDescriptionIndex)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;
	return Media_IsSelfContained(trak->Media, sampleDescriptionIndex);
}

/*retrieves given sample DTS*/
GF_EXPORT
u32 gf_isom_get_sample_from_dts(GF_ISOFile *the_file, u32 trackNumber, u64 dts)
{
	GF_Err e;
	u32 sampleNumber, prevSampleNumber;
	GF_TrackBox *trak;
	GF_SampleTableBox *stbl;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	stbl = trak->Media->information->sampleTable;

	e = stbl_findEntryForTime(stbl, dts, 1, &sampleNumber, &prevSampleNumber);
	if (e) return 0;
	return sampleNumber;
}


//return a sample given a desired display time IN MEDIA TIME SCALE
//and set the StreamDescIndex of this sample
//this index allows to retrieve the stream description if needed (2 media in 1 track)
//return NULL if error
//WARNING: the sample may not be sync even though the sync was requested (depends on the media)
GF_EXPORT
GF_Err gf_isom_get_sample_for_media_time(GF_ISOFile *the_file, u32 trackNumber, u64 desiredTime, u32 *StreamDescriptionIndex, GF_ISOSearchMode SearchMode, GF_ISOSample **sample, u32 *SampleNum, u64 *data_offset)
{
	GF_Err e;
	u32 sampleNumber, prevSampleNumber, syncNum, shadowSync;
	GF_TrackBox *trak;
	GF_ISOSample *shadow;
	GF_SampleTableBox *stbl;
	Bool static_sample = GF_FALSE;
	u8 useShadow, IsSync;

	if (SampleNum) *SampleNum = 0;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stbl = trak->Media->information->sampleTable;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (desiredTime < trak->dts_at_seg_start) {
		desiredTime = 0;
	} else {
		desiredTime -= trak->dts_at_seg_start;
	}
#endif

	e = stbl_findEntryForTime(stbl, desiredTime, 0, &sampleNumber, &prevSampleNumber);
	if (e) return e;

	//if no shadow table, reset to sync only
	useShadow = 0;
	if (!stbl->ShadowSync && (SearchMode == GF_ISOM_SEARCH_SYNC_SHADOW))
		SearchMode = GF_ISOM_SEARCH_SYNC_BACKWARD;

	//if no syncTable, disable syncSearching, as all samples ARE sync
	if (! trak->Media->information->sampleTable->SyncSample) {
		if (SearchMode == GF_ISOM_SEARCH_SYNC_FORWARD) SearchMode = GF_ISOM_SEARCH_FORWARD;
		if (SearchMode == GF_ISOM_SEARCH_SYNC_BACKWARD) SearchMode = GF_ISOM_SEARCH_BACKWARD;
	}

	//not found, return EOF or browse backward
	if (!sampleNumber && !prevSampleNumber) {
		if (SearchMode == GF_ISOM_SEARCH_SYNC_BACKWARD || SearchMode == GF_ISOM_SEARCH_BACKWARD) {
			sampleNumber = trak->Media->information->sampleTable->SampleSize->sampleCount;
		}
		if (!sampleNumber) return GF_EOS;
	}

	//check in case we have the perfect sample
	IsSync = 0;

	//according to the direction adjust the sampleNum value
	switch (SearchMode) {
	case GF_ISOM_SEARCH_SYNC_FORWARD:
		IsSync = 1;
	case GF_ISOM_SEARCH_FORWARD:
		//not the exact one
		if (!sampleNumber) {
			if (prevSampleNumber != stbl->SampleSize->sampleCount) {
				sampleNumber = prevSampleNumber + 1;
			} else {
				sampleNumber = prevSampleNumber;
			}
		}
		break;

	//if dummy mode, reset to default browsing
	case GF_ISOM_SEARCH_SYNC_BACKWARD:
		IsSync = 1;
	case GF_ISOM_SEARCH_SYNC_SHADOW:
	case GF_ISOM_SEARCH_BACKWARD:
	default:
		//first case, not found....
		if (!sampleNumber && !prevSampleNumber) {
			sampleNumber = stbl->SampleSize->sampleCount;
		} else if (!sampleNumber) {
			sampleNumber = prevSampleNumber;
		}
		break;
	}

	//get the sync sample num
	if (IsSync) {
		//get the SyncNumber
		e = Media_FindSyncSample(trak->Media->information->sampleTable,
		                         sampleNumber, &syncNum, SearchMode);
		if (e) return e;
		if (syncNum) sampleNumber = syncNum;
		syncNum = 0;
	}
	//if we are in shadow mode, get the previous sync sample
	//in case we can't find a good SyncShadow
	else if (SearchMode == GF_ISOM_SEARCH_SYNC_SHADOW) {
		//get the SyncNumber
		e = Media_FindSyncSample(trak->Media->information->sampleTable,
		                         sampleNumber, &syncNum, GF_ISOM_SEARCH_SYNC_BACKWARD);
		if (e) return e;
	}


	//OK sampleNumber is exactly the sample we need (except for shadow)

	if (sample) {
		if (*sample) {
			static_sample = GF_TRUE;
		} else {
			*sample = gf_isom_sample_new();
			if (*sample == NULL) return GF_OUT_OF_MEM;
		}
	}
	//we are in shadow mode, we need to browse both SyncSample and ShadowSyncSample to get
	//the desired sample...
	if (SearchMode == GF_ISOM_SEARCH_SYNC_SHADOW) {
		//get the shadowing number
		stbl_GetSampleShadow(stbl->ShadowSync, &sampleNumber, &shadowSync);
		//now sampleNumber is the closest previous shadowed sample.
		//1- If we have a closer sync sample, use it.
		//2- if the shadowSync is 0, we don't have any shadowing, use syncNum
		if ((sampleNumber < syncNum) || (!shadowSync)) {
			sampleNumber = syncNum;
		} else {
			//otherwise, we have a better alternate sample in the shadowSync for this sample
			useShadow = 1;
		}
	}

	e = Media_GetSample(trak->Media, sampleNumber, sample, StreamDescriptionIndex, GF_FALSE, data_offset);
	if (e) {
		if (!static_sample)
			gf_isom_sample_del(sample);
		else if (! (*sample)->alloc_size && (*sample)->data && (*sample)->dataLength )
		 	(*sample)->alloc_size =  (*sample)->dataLength;

		return e;
	}
	if (sample && ! (*sample)->IsRAP) {
		Bool is_rap;
		GF_ISOSampleRollType roll_type;
		e = gf_isom_get_sample_rap_roll_info(the_file, trackNumber, sampleNumber, &is_rap, &roll_type, NULL);
		if (e) return e;
		if (is_rap) (*sample)->IsRAP = SAP_TYPE_3;
	}
	//optionally get the sample number
	if (SampleNum) {
		*SampleNum = sampleNumber;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
		*SampleNum += trak->sample_count_at_seg_start;
#endif
	}

	//in shadow mode, we only get the data of the shadowing sample !
	if (sample && useShadow) {
		//we have to use StreamDescriptionIndex in case the sample data is in another desc
		//though this is unlikely as non optimized...
		shadow = gf_isom_get_sample(the_file, trackNumber, shadowSync, StreamDescriptionIndex);
		//if no sample, the shadowSync is broken, return the sample
		if (!shadow) return GF_OK;
		(*sample)->IsRAP = RAP;
		gf_free((*sample)->data);
		(*sample)->dataLength = shadow->dataLength;
		(*sample)->data = shadow->data;
		//set data length to 0 to keep the buffer alive...
		shadow->dataLength = 0;
		gf_isom_sample_del(&shadow);
	}
	if (static_sample && ! (*sample)->alloc_size )
		 (*sample)->alloc_size =  (*sample)->dataLength;

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_sample_for_movie_time(GF_ISOFile *the_file, u32 trackNumber, u64 movieTime, u32 *StreamDescriptionIndex, GF_ISOSearchMode SearchMode, GF_ISOSample **sample, u32 *sampleNumber, u64 *data_offset)
{
	Double tsscale;
	GF_Err e;
	GF_TrackBox *trak;
	u64 mediaTime, nextMediaTime;
	s64 segStartTime, mediaOffset;
	u32 sampNum;
	u8 useEdit;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	//only check duration if initially set - do not check duration as updated after fragment merge since that duration does not take
	//into account tfdt
	if (trak->Header->initial_duration
		&& gf_timestamp_greater(movieTime, trak->Media->mediaHeader->timeScale, trak->Header->initial_duration, trak->moov->mvhd->timeScale)
	) {
		if (sampleNumber) *sampleNumber = 0;
		*StreamDescriptionIndex = 0;
		return GF_EOS;
	}

	//get the media time for this movie time...
	mediaTime = segStartTime = 0;
	*StreamDescriptionIndex = 0;
	nextMediaTime = 0;

	e = GetMediaTime(trak, (SearchMode==GF_ISOM_SEARCH_SYNC_FORWARD) ? GF_TRUE : GF_FALSE, movieTime, &mediaTime, &segStartTime, &mediaOffset, &useEdit, &nextMediaTime);
	if (e) return e;

	/*here we check if we were playing or not and return no sample in normal search modes*/
	if (useEdit && mediaOffset == -1) {
		if ((SearchMode==GF_ISOM_SEARCH_FORWARD) || (SearchMode==GF_ISOM_SEARCH_BACKWARD)) {
			/*get next sample time in MOVIE timescale*/
			if (SearchMode==GF_ISOM_SEARCH_FORWARD)
				e = GetNextMediaTime(trak, movieTime, &mediaTime);
			else
				e = GetPrevMediaTime(trak, movieTime, &mediaTime);
			if (e) return e;
			return gf_isom_get_sample_for_movie_time(the_file, trackNumber, (u32) mediaTime, StreamDescriptionIndex, GF_ISOM_SEARCH_SYNC_FORWARD, sample, sampleNumber, data_offset);
		}
		if (sampleNumber) *sampleNumber = 0;
		if (sample) {
			if (! (*sample)) {
				*sample = gf_isom_sample_new();
				if (! *sample) return GF_OUT_OF_MEM;
			}
			(*sample)->DTS = movieTime;
			(*sample)->dataLength = 0;
			(*sample)->CTS_Offset = 0;
		}
		return GF_OK;
	}
	/*dwell edit in non-sync mode, fetch next/prev sample depending on mode.
	Otherwise return the dwell entry*/
	if (useEdit==2) {
		if ((SearchMode==GF_ISOM_SEARCH_FORWARD) || (SearchMode==GF_ISOM_SEARCH_BACKWARD)) {
			/*get next sample time in MOVIE timescale*/
			if (SearchMode==GF_ISOM_SEARCH_FORWARD)
				e = GetNextMediaTime(trak, movieTime, &mediaTime);
			else
				e = GetPrevMediaTime(trak, movieTime, &mediaTime);
			if (e) return e;
			return gf_isom_get_sample_for_movie_time(the_file, trackNumber, (u32) mediaTime, StreamDescriptionIndex, GF_ISOM_SEARCH_SYNC_FORWARD, sample, sampleNumber, data_offset);
		}
	}

	tsscale = trak->Media->mediaHeader->timeScale;
	tsscale /= trak->moov->mvhd->timeScale;

	//OK, we have a sample so fetch it
	e = gf_isom_get_sample_for_media_time(the_file, trackNumber, mediaTime, StreamDescriptionIndex, SearchMode, sample, &sampNum, data_offset);
	if (e) {
		if (e==GF_EOS) {
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			//movie is fragmented and samples not yet received, return EOS
			if (the_file->moov->mvex && !trak->Media->information->sampleTable->SampleSize->sampleCount)
				return e;
#endif

			if ((SearchMode==GF_ISOM_SEARCH_SYNC_BACKWARD) || (SearchMode==GF_ISOM_SEARCH_BACKWARD)) {
				if (nextMediaTime && (nextMediaTime-1 < movieTime))
					return gf_isom_get_sample_for_movie_time(the_file, trackNumber, nextMediaTime-1, StreamDescriptionIndex, SearchMode, sample, sampleNumber, data_offset);
			} else {
				if (nextMediaTime && (nextMediaTime-1 > movieTime))
					return gf_isom_get_sample_for_movie_time(the_file, trackNumber, nextMediaTime-1, StreamDescriptionIndex, SearchMode, sample, sampleNumber, data_offset);
			}
		}
		return e;
	}

	//OK, now the trick: we have to rebuild the time stamps, according
	//to the media time scale (used by SLConfig) - add the edit start time but stay in
	//the track TS
	if (sample && useEdit) {
		u64 _ts = (u64)(segStartTime * tsscale);

		(*sample)->DTS += _ts;
		/*watchout, the sample fetched may be before the first sample in the edit list (when seeking)*/
		if ( (*sample)->DTS > (u64) mediaOffset) {
			(*sample)->DTS -= (u64) mediaOffset;
		} else {
			(*sample)->DTS = 0;
		}
	}
	if (sampleNumber) *sampleNumber = sampNum;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sample && (*sample) ) (*sample)->DTS += trak->dts_at_seg_start;
#endif

	return GF_OK;
}



GF_EXPORT
u64 gf_isom_get_missing_bytes(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	return trak->Media->BytesMissing;
}

GF_EXPORT
GF_Err gf_isom_set_sample_padding(GF_ISOFile *the_file, u32 trackNumber, u32 padding_bytes)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	trak->padding_bytes = padding_bytes;
	return GF_OK;

}

//get the number of edited segment
GF_EXPORT
Bool gf_isom_get_edit_list_type(GF_ISOFile *the_file, u32 trackNumber, s64 *mediaOffset)
{
	GF_EdtsEntry *ent;
	GF_TrackBox *trak;
	u32 count;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;
	*mediaOffset = 0;
	if (!trak->editBox || !trak->editBox->editList) return GF_FALSE;

	count = gf_list_count(trak->editBox->editList->entryList);
	ent = (GF_EdtsEntry*)gf_list_get(trak->editBox->editList->entryList, 0);
	if (!ent) return GF_TRUE;
	/*mediaRate>0, the track playback shall start at media time>0 -> mediaOffset is < 0 */
	if ((count==1) && (ent->mediaRate == 0x10000) && (ent->mediaTime>=0)) {
		*mediaOffset = - ent->mediaTime;
		return GF_FALSE;
	} else if (count==2) {
		/*mediaTime==-1, the track playback shall be empty for segmentDuration -> mediaOffset is > 0 */
		if ((ent->mediaRate == -0x10000) || (ent->mediaTime==-1)) {
			Double time = (Double) ent->segmentDuration;
			time /= trak->moov->mvhd->timeScale;
			time *= trak->Media->mediaHeader->timeScale;
			*mediaOffset = (s64) time;

			//check next entry, if we start from mediaOffset > 0 this may still result in a skip
			ent = (GF_EdtsEntry*)gf_list_get(trak->editBox->editList->entryList, 1);
			//next entry playback rate is not nominal, we need edit list handling
			if (ent->mediaRate != 0x10000)
				return GF_TRUE;

			if (ent->mediaTime > 0) {
				*mediaOffset -= ent->mediaTime;
			}
			return GF_FALSE;
		}
	}
	return GF_TRUE;
}


//get the number of edited segment
GF_EXPORT
u32 gf_isom_get_edits_count(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;

	if (!trak->editBox || !trak->editBox->editList) return 0;
	return gf_list_count(trak->editBox->editList->entryList);
}


//Get the desired segment information
GF_EXPORT
GF_Err gf_isom_get_edit(GF_ISOFile *the_file, u32 trackNumber, u32 SegmentIndex, u64 *EditTime, u64 *SegmentDuration, u64 *MediaTime, GF_ISOEditType *EditMode)
{
	u32 i;
	u64 startTime;
	GF_TrackBox *trak;
	GF_EditListBox *elst;
	GF_EdtsEntry *ent;

	ent = NULL;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (!trak->editBox ||
	        !trak->editBox->editList ||
	        (SegmentIndex > gf_list_count(trak->editBox->editList->entryList)) ||
	        !SegmentIndex)
		return GF_BAD_PARAM;

	elst = trak->editBox->editList;
	startTime = 0;

	for (i = 0; i < SegmentIndex; i++) {
		ent = (GF_EdtsEntry*)gf_list_get(elst->entryList, i);
		if (i < SegmentIndex-1) startTime += ent->segmentDuration;
	}
	*EditTime = startTime;
	*SegmentDuration = ent->segmentDuration;
	if (ent->mediaTime < 0) {
		*MediaTime = 0;
		*EditMode = GF_ISOM_EDIT_EMPTY;
		return GF_OK;
	}
	if (ent->mediaRate == 0) {
		*MediaTime = ent->mediaTime;
		*EditMode = GF_ISOM_EDIT_DWELL;
		return GF_OK;
	}
	*MediaTime = ent->mediaTime;
	*EditMode = GF_ISOM_EDIT_NORMAL;
	return GF_OK;
}

GF_EXPORT
u8 gf_isom_has_sync_points(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable) return 0;
	if (trak->Media->information->sampleTable->SyncSample) {
		if (!trak->Media->information->sampleTable->SyncSample->nb_entries) return 2;
		return 1;
	}
	return 0;
}

/*returns number of sync points*/
GF_EXPORT
u32 gf_isom_get_sync_point_count(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;
	if (trak->Media->information->sampleTable->SyncSample) {
		return trak->Media->information->sampleTable->SyncSample->nb_entries;
	}
	return 0;
}


GF_EXPORT
GF_Err gf_isom_get_brand_info(GF_ISOFile *movie, u32 *brand, u32 *minorVersion, u32 *AlternateBrandsCount)
{
	if (!movie) return GF_BAD_PARAM;
	if (!movie->brand) {
		if (brand) *brand = GF_ISOM_BRAND_ISOM;
		if (minorVersion) *minorVersion = 1;
		if (AlternateBrandsCount) *AlternateBrandsCount = 0;
		return GF_OK;
	}

	if (brand) *brand = movie->brand->majorBrand;
	if (minorVersion) *minorVersion = movie->brand->minorVersion;
	if (AlternateBrandsCount) *AlternateBrandsCount = movie->brand->altCount;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_alternate_brand(GF_ISOFile *movie, u32 BrandIndex, u32 *brand)
{
	if (!movie || !movie->brand || !brand) return GF_BAD_PARAM;
	if (BrandIndex > movie->brand->altCount || !BrandIndex) return GF_BAD_PARAM;
	*brand = movie->brand->altBrand[BrandIndex-1];
	return GF_OK;
}

GF_EXPORT
const u32 *gf_isom_get_brands(GF_ISOFile *movie)
{
	if (!movie || !movie->brand) return NULL;
	return movie->brand->altBrand;
}

GF_EXPORT
GF_Err gf_isom_get_sample_padding_bits(GF_ISOFile *the_file, u32 trackNumber, u32 sampleNumber, u8 *NbBits)
{
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;


	//Padding info
	return stbl_GetPaddingBits(trak->Media->information->sampleTable->PaddingBits,
	                           sampleNumber, NbBits);

}


GF_EXPORT
Bool gf_isom_has_padding_bits(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;

	if (trak->Media->information->sampleTable->PaddingBits) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
u32 gf_isom_get_udta_count(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak;
	GF_UserDataBox *udta;
	if (!movie || !movie->moov) return 0;

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return 0;
		udta = trak->udta;
	} else {
		udta = movie->moov->udta;
	}
	if (udta) return gf_list_count(udta->recordList);
	return 0;
}

GF_EXPORT
GF_Err gf_isom_get_udta_type(GF_ISOFile *movie, u32 trackNumber, u32 udta_idx, u32 *UserDataType, bin128 *UUID)
{
	GF_TrackBox *trak;
	GF_UserDataBox *udta;
	GF_UserDataMap *map;
	if (!movie || !movie->moov || !udta_idx) return GF_BAD_PARAM;

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_OK;
		udta = trak->udta;
	} else {
		udta = movie->moov->udta;
	}
	if (!udta) return GF_BAD_PARAM;
	if (udta_idx>gf_list_count(udta->recordList)) return GF_BAD_PARAM;
	map = (GF_UserDataMap*)gf_list_get(udta->recordList, udta_idx - 1);
	if (UserDataType) *UserDataType = map->boxType;
	if (UUID) memcpy(*UUID, map->uuid, 16);
	return GF_OK;
}

GF_EXPORT
u32 gf_isom_get_user_data_count(GF_ISOFile *movie, u32 trackNumber, u32 UserDataType, bin128 UUID)
{
	GF_UserDataMap *map;
	GF_TrackBox *trak;
	GF_UserDataBox *udta;
	bin128 t;
	u32 i, count;

	if (!movie || !movie->moov) return 0;

	if (UserDataType == GF_ISOM_BOX_TYPE_UUID) UserDataType = 0;
	memset(t, 1, 16);

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return 0;
		udta = trak->udta;
	} else {
		udta = movie->moov->udta;
	}
	if (!udta) return 0;

	i=0;
	while ((map = (GF_UserDataMap*)gf_list_enum(udta->recordList, &i))) {
		count = gf_list_count(map->boxes);

		if ((map->boxType == GF_ISOM_BOX_TYPE_UUID) && !memcmp(map->uuid, UUID, 16)) return count;
		else if (map->boxType == UserDataType) return count;
	}
	return 0;
}

GF_EXPORT
GF_Err gf_isom_get_user_data(GF_ISOFile *movie, u32 trackNumber, u32 UserDataType, bin128 UUID, u32 UserDataIndex, u8 **userData, u32 *userDataSize)
{
	GF_UserDataMap *map;
	GF_UnknownBox *ptr;
	GF_BitStream *bs;
	u32 i;
	bin128 t;
	GF_TrackBox *trak;
	GF_UserDataBox *udta;

	if (!movie || !movie->moov) return GF_BAD_PARAM;

	if (trackNumber) {
		trak = gf_isom_get_track_from_file(movie, trackNumber);
		if (!trak) return GF_BAD_PARAM;
		udta = trak->udta;
	} else {
		udta = movie->moov->udta;
	}
	if (!udta) return GF_BAD_PARAM;

	if (UserDataType == GF_ISOM_BOX_TYPE_UUID) UserDataType = 0;
	memset(t, 1, 16);

	if (!userData || !userDataSize || *userData) return GF_BAD_PARAM;

	i=0;
	while ((map = (GF_UserDataMap*)gf_list_enum(udta->recordList, &i))) {
		if ((map->boxType == GF_ISOM_BOX_TYPE_UUID) && !memcmp(map->uuid, UUID, 16)) goto found;
		else if (map->boxType == UserDataType) goto found;

	}
	return GF_BAD_PARAM;

found:
	if (UserDataIndex) {
		if (UserDataIndex > gf_list_count(map->boxes) ) return GF_BAD_PARAM;
		ptr = (GF_UnknownBox*)gf_list_get(map->boxes, UserDataIndex-1);

		if (ptr->type == GF_ISOM_BOX_TYPE_UNKNOWN) {
			if (!ptr->dataSize) {
				*userData = NULL;
				*userDataSize = 0;
				return GF_OK;
			}
			*userData = (char *)gf_malloc(sizeof(char)*ptr->dataSize);
			if (!*userData) return GF_OUT_OF_MEM;
			memcpy(*userData, ptr->data, sizeof(char)*ptr->dataSize);
			*userDataSize = ptr->dataSize;
			return GF_OK;
		} else if (ptr->type == GF_ISOM_BOX_TYPE_UUID) {
			GF_UnknownUUIDBox *p_uuid = (GF_UnknownUUIDBox *)ptr;
			if (!p_uuid->dataSize) {
				*userData = NULL;
				*userDataSize = 0;
				return GF_OK;
			}
			*userData = (char *)gf_malloc(sizeof(char)*p_uuid->dataSize);
			if (!*userData) return GF_OUT_OF_MEM;
			memcpy(*userData, p_uuid->data, sizeof(char)*p_uuid->dataSize);
			*userDataSize = p_uuid->dataSize;
			return GF_OK;
		} else {
			char *str = NULL;
			switch (ptr->type) {
			case GF_ISOM_BOX_TYPE_NAME:
			//case GF_QT_BOX_TYPE_NAME: same as above
				str = ((GF_NameBox *)ptr)->string;
				break;
			case GF_ISOM_BOX_TYPE_KIND:
				str = ((GF_KindBox *)ptr)->value;
				break;
			}
			if (str) {
				u32 len = (u32) strlen(str) + 1;
				*userData = (char *)gf_malloc(sizeof(char) * len);
				if (!*userData) return GF_OUT_OF_MEM;
				memcpy(*userData, str, sizeof(char)*len);
				*userDataSize = len;
				return GF_OK;
			}
			return GF_NOT_SUPPORTED;
		}
	}

	//serialize all boxes
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	i=0;
	while ( (ptr = (GF_UnknownBox*)gf_list_enum(map->boxes, &i))) {
		u32 type, s, data_size;
		char *data=NULL;
		if (ptr->type == GF_ISOM_BOX_TYPE_UNKNOWN) {
			type = ptr->original_4cc;
			data_size = ptr->dataSize;
			data = ptr->data;
		} else if (ptr->type == GF_ISOM_BOX_TYPE_UUID) {
			GF_UnknownUUIDBox *p_uuid = (GF_UnknownUUIDBox *)ptr;
			type = p_uuid->type;
			data_size = p_uuid->dataSize;
			data = p_uuid->data;
		} else {
			gf_isom_box_write((GF_Box *)ptr, bs);
			continue;
		}
		s = data_size+8;
		if (ptr->type==GF_ISOM_BOX_TYPE_UUID) s += 16;

		gf_bs_write_u32(bs, s);
		gf_bs_write_u32(bs, type);
		if (type==GF_ISOM_BOX_TYPE_UUID) gf_bs_write_data(bs, (char *) map->uuid, 16);
		if (data) {
			gf_bs_write_data(bs, data, data_size);
		} else if (ptr->child_boxes) {
#ifndef GPAC_DISABLE_ISOM_WRITE
			gf_isom_box_array_write((GF_Box *)ptr, ptr->child_boxes, bs);
#else
			GF_LOG(GF_LOG_WARNING, GF_LOG_CONTAINER, ("ISOBMF: udta is a box-list - cannot export in read-only version of libisom in GPAC\n" ));
#endif
		}
	}
	gf_bs_get_content(bs, userData, userDataSize);
	gf_bs_del(bs);
	return GF_OK;
}

GF_EXPORT
void gf_isom_delete(GF_ISOFile *movie)
{
	//free and return;
	gf_isom_delete_movie(movie);
}

GF_EXPORT
GF_Err gf_isom_get_chunks_infos(GF_ISOFile *movie, u32 trackNumber, u32 *dur_min, u32 *dur_avg, u32 *dur_max, u32 *size_min, u32 *size_avg, u32 *size_max)
{
	GF_TrackBox *trak;
	u32 i, k, sample_idx, dmin, dmax, smin, smax, tot_chunks;
	u64 davg, savg;
	GF_SampleToChunkBox *stsc;
	GF_TimeToSampleBox *stts;
	if (!movie || !trackNumber || !movie->moov) return GF_BAD_PARAM;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsc = trak->Media->information->sampleTable->SampleToChunk;
	stts = trak->Media->information->sampleTable->TimeToSample;
	if (!stsc || !stts) return GF_ISOM_INVALID_FILE;

	dmin = smin = (u32) -1;
	dmax = smax = 0;
	davg = savg = 0;
	sample_idx = 1;
	tot_chunks = 0;
	for (i=0; i<stsc->nb_entries; i++) {
		u32 nb_chunk = 0;
		if (stsc->entries[i].samplesPerChunk >  2*trak->Media->information->sampleTable->SampleSize->sampleCount) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[iso file] likely broken stco entry (%u samples per chunk but %u samples total)\n", stsc->entries[i].samplesPerChunk, trak->Media->information->sampleTable->SampleSize->sampleCount));
			return GF_ISOM_INVALID_FILE;
		}
		while (1) {
			u32 chunk_dur = 0;
			u32 chunk_size = 0;
			for (k=0; k<stsc->entries[i].samplesPerChunk; k++) {
				u64 dts;
				u32 dur;
				u32 size;
				stbl_GetSampleDTS_and_Duration(stts, k+sample_idx, &dts, &dur);
				chunk_dur += dur;
				stbl_GetSampleSize(trak->Media->information->sampleTable->SampleSize, k+sample_idx, &size);
				chunk_size += size;

			}
			if (dmin>chunk_dur) dmin = chunk_dur;
			if (dmax<chunk_dur) dmax = chunk_dur;
			davg += chunk_dur;
			if (smin>chunk_size) smin = chunk_size;
			if (smax<chunk_size) smax = chunk_size;
			savg += chunk_size;

			tot_chunks ++;
			sample_idx += stsc->entries[i].samplesPerChunk;
			if (i+1==stsc->nb_entries) break;
			nb_chunk ++;
			if (stsc->entries[i].firstChunk + nb_chunk == stsc->entries[i+1].firstChunk) break;
		}
	}
	if (tot_chunks) {
		davg /= tot_chunks;
		savg /= tot_chunks;
	}
	if (dur_min) *dur_min = dmin;
	if (dur_avg) *dur_avg = (u32) davg;
	if (dur_max) *dur_max = dmax;

	if (size_min) *size_min = smin;
	if (size_avg) *size_avg = (u32) savg;
	if (size_max) *size_max = smax;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_fragment_defaults(GF_ISOFile *the_file, u32 trackNumber,
                                     u32 *defaultDuration, u32 *defaultSize, u32 *defaultDescriptionIndex,
                                     u32 *defaultRandomAccess, u8 *defaultPadding, u16 *defaultDegradationPriority)
{
	GF_TrackBox *trak;
	GF_StscEntry *sc_ent;
	u32 i, j, maxValue, value;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	GF_TrackExtendsBox *trex;
#endif
	GF_SampleTableBox *stbl;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	/*if trex is already set, restore flags*/
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	trex = the_file->moov->mvex ? GetTrex(the_file->moov, gf_isom_get_track_id(the_file,trackNumber) ) : NULL;
	if (trex) {
		trex->track = trak;

		if (defaultDuration) *defaultDuration = trex->def_sample_duration;
		if (defaultSize) *defaultSize = trex->def_sample_size;
		if (defaultDescriptionIndex) *defaultDescriptionIndex = trex->def_sample_desc_index;
		if (defaultRandomAccess) *defaultRandomAccess = GF_ISOM_GET_FRAG_SYNC(trex->def_sample_flags);
		if (defaultPadding) *defaultPadding = GF_ISOM_GET_FRAG_PAD(trex->def_sample_flags);
		if (defaultDegradationPriority) *defaultDegradationPriority = GF_ISOM_GET_FRAG_DEG(trex->def_sample_flags);
		return GF_OK;
	}
#endif

	stbl = trak->Media->information->sampleTable;
	if (!stbl->TimeToSample || !stbl->SampleSize || !stbl->SampleToChunk) return GF_ISOM_INVALID_FILE;


	//duration
	if (defaultDuration) {
		maxValue = value = 0;
		for (i=0; i<stbl->TimeToSample->nb_entries; i++) {
			if (stbl->TimeToSample->entries[i].sampleCount>maxValue) {
				value = stbl->TimeToSample->entries[i].sampleDelta;
				maxValue = stbl->TimeToSample->entries[i].sampleCount;
			}
		}
		*defaultDuration = value;
	}
	//size
	if (defaultSize) {
		*defaultSize = stbl->SampleSize->sampleSize;
	}
	//descIndex
	if (defaultDescriptionIndex) {
		GF_SampleToChunkBox *stsc= stbl->SampleToChunk;
		maxValue = value = 0;
		for (i=0; i<stsc->nb_entries; i++) {
			sc_ent = &stsc->entries[i];
			if ((sc_ent->nextChunk - sc_ent->firstChunk) * sc_ent->samplesPerChunk > maxValue) {
				value = sc_ent->sampleDescriptionIndex;
				maxValue = (sc_ent->nextChunk - sc_ent->firstChunk) * sc_ent->samplesPerChunk;
			}
		}
		*defaultDescriptionIndex = value ? value : 1;
	}
	//RAP
	if (defaultRandomAccess) {
		//no sync table is ALL RAP
		*defaultRandomAccess = stbl->SyncSample ? 0 : 1;
		if (stbl->SyncSample
		        && (stbl->SyncSample->nb_entries == stbl->SampleSize->sampleCount)) {
			*defaultRandomAccess = 1;
		}
	}
	//defaultPadding
	if (defaultPadding) {
		*defaultPadding = 0;
		if (stbl->PaddingBits) {
			maxValue = 0;
			for (i=0; i<stbl->PaddingBits->SampleCount; i++) {
				value = 0;
				for (j=0; j<stbl->PaddingBits->SampleCount; j++) {
					if (stbl->PaddingBits->padbits[i]==stbl->PaddingBits->padbits[j]) {
						value ++;
					}
				}
				if (value>maxValue) {
					maxValue = value;
					*defaultPadding = stbl->PaddingBits->padbits[i];
				}
			}
		}
	}
	//defaultDegradationPriority
	if (defaultDegradationPriority) {
		*defaultDegradationPriority = 0;
		if (stbl->DegradationPriority) {
			maxValue = 0;
			for (i=0; i<stbl->DegradationPriority->nb_entries; i++) {
				value = 0;
				for (j=0; j<stbl->DegradationPriority->nb_entries; j++) {
					if (stbl->DegradationPriority->priorities[i]==stbl->DegradationPriority->priorities[j]) {
						value ++;
					}
				}
				if (value>maxValue) {
					maxValue = value;
					*defaultDegradationPriority = stbl->DegradationPriority->priorities[i];
				}
			}
		}
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_refresh_fragmented(GF_ISOFile *movie, u64 *MissingBytes, const char *new_location)
{
#ifdef	GPAC_DISABLE_ISOM_FRAGMENTS
	return GF_NOT_SUPPORTED;
#else
	u64 prevsize, size;
	u32 i;
	if (!movie || !movie->movieFileMap || !movie->moov) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_READ) return GF_BAD_PARAM;

	/*refresh size*/
	size = movie->movieFileMap ? gf_bs_get_size(movie->movieFileMap->bs) : 0;

	if (new_location) {
		Bool delete_map;
		GF_DataMap *previous_movie_fileMap_address = movie->movieFileMap;
		GF_Err e;

		e = gf_isom_datamap_new(new_location, NULL, GF_ISOM_DATA_MAP_READ_ONLY, &movie->movieFileMap);
		if (e) {
			movie->movieFileMap = previous_movie_fileMap_address;
			return e;
		}

		delete_map = (previous_movie_fileMap_address != NULL ? GF_TRUE: GF_FALSE);
		for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
			GF_TrackBox *trak = (GF_TrackBox *)gf_list_get(movie->moov->trackList, i);
			if (trak->Media->information->dataHandler == previous_movie_fileMap_address) {
				//reaasign for later destruction
				trak->Media->information->scalableDataHandler = movie->movieFileMap;
				//reassign for Media_GetSample function
				trak->Media->information->dataHandler = movie->movieFileMap;
			} else if (trak->Media->information->scalableDataHandler == previous_movie_fileMap_address) {
				delete_map = GF_FALSE;
			}
		}
		if (delete_map) {
			gf_isom_datamap_del(previous_movie_fileMap_address);
		}
	}

	prevsize = gf_bs_get_refreshed_size(movie->movieFileMap->bs);
	if (prevsize==size) return GF_OK;

	if (!movie->moov->mvex)
		return GF_OK;

	//ok parse root boxes
	return gf_isom_parse_movie_boxes(movie, NULL, MissingBytes, GF_TRUE);
#endif
}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
GF_EXPORT
void gf_isom_set_single_moof_mode(GF_ISOFile *movie, Bool mode)
{
	movie->single_moof_mode = mode;
}
#endif

GF_EXPORT
GF_Err gf_isom_reset_data_offset(GF_ISOFile *movie, u64 *top_box_start)
{
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	u32 i, count;
	if (!movie || !movie->moov) return GF_BAD_PARAM;
	if (top_box_start) *top_box_start = movie->current_top_box_start;
	movie->current_top_box_start = 0;
	movie->NextMoofNumber = 0;
	if (movie->moov->mvex && movie->single_moof_mode) {
		movie->single_moof_state = 0;
	}
	count = gf_list_count(movie->moov->trackList);
	for (i=0; i<count; i++) {
		GF_TrackBox *tk = gf_list_get(movie->moov->trackList, i);
		tk->first_traf_merged = GF_FALSE;
	}
#endif
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_current_top_box_offset(GF_ISOFile *movie, u64 *current_top_box_offset)
{
	if (!movie || !movie->moov || !current_top_box_offset) return GF_BAD_PARAM;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	*current_top_box_offset = movie->current_top_box_start;
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

GF_EXPORT
GF_Err gf_isom_set_removed_bytes(GF_ISOFile *movie, u64 bytes_removed)
{
	if (!movie || !movie->moov) return GF_BAD_PARAM;
	movie->bytes_removed = bytes_removed;
	return GF_OK;
}

GF_Err gf_isom_purge_samples(GF_ISOFile *the_file, u32 trackNumber, u32 nb_samples)
{
	GF_TrackBox *trak;
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	GF_Err e;
	GF_TrackExtendsBox *trex;
	GF_SampleTableBox *stbl;
#endif
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	/*if trex is already set, restore flags*/
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	trex = the_file->moov->mvex ? GetTrex(the_file->moov, gf_isom_get_track_id(the_file,trackNumber) ) : NULL;
	if (!trex) return GF_BAD_PARAM;

	//first unpack chunk offsets and CTS
	e = stbl_UnpackOffsets(trak->Media->information->sampleTable);
	if (e) return e;
	e = stbl_unpackCTS(trak->Media->information->sampleTable);
	if (e) return e;

	stbl = trak->Media->information->sampleTable;
	if (!stbl->TimeToSample || !stbl->SampleSize || !stbl->SampleToChunk) return GF_ISOM_INVALID_FILE;

	//remove at once nb_samples in stts, ctts, stsz, stco, stsc and stdp (n-times removal is way too slow)
	//do NOT change the order DTS, CTS, size chunk
	stbl_RemoveDTS(stbl, 1, nb_samples, 0);
	stbl_RemoveCTS(stbl, 1, nb_samples);
	stbl_RemoveSize(stbl, 1, nb_samples);
	stbl_RemoveChunk(stbl, 1, nb_samples);
	stbl_RemoveRedundant(stbl, 1, nb_samples);
	stbl_RemoveRAPs(stbl, nb_samples);

	//then remove sample per sample for the rest, which is either
	//- sparse data
	//- allocated structure rather than memmove-able array
	//- not very frequent info (paddind bits)
	while (nb_samples) {
		stbl_RemoveShadow(stbl, 1);
		stbl_RemoveSubSample(stbl, 1);
		stbl_RemovePaddingBits(stbl, 1);
		stbl_RemoveSampleGroup(stbl, 1);
		nb_samples--;
	}
	return GF_OK;
#else
	return GF_NOT_SUPPORTED;
#endif
}

//reset SampleTable boxes, but do not destroy them if memory reuse is possible
//this reduces free/alloc time when many fragments
static void gf_isom_recreate_tables(GF_TrackBox *trak)
{
	u32 j;
	GF_Box *a;
	GF_SampleTableBox *stbl = trak->Media->information->sampleTable;

	if (stbl->ChunkOffset) {
		if (stbl->ChunkOffset->type==GF_ISOM_BOX_TYPE_CO64) {
			GF_ChunkLargeOffsetBox *co64 = (GF_ChunkLargeOffsetBox *)stbl->ChunkOffset;
			co64->nb_entries = 0;
		} else {
			GF_ChunkOffsetBox *stco = (GF_ChunkOffsetBox *)stbl->ChunkOffset;
			stco->nb_entries = 0;
		}
	}

	if (stbl->CompositionOffset) {
		stbl->CompositionOffset->nb_entries = 0;
		stbl->CompositionOffset->w_LastSampleNumber = 0;
		stbl->CompositionOffset->r_currentEntryIndex = 0;
		stbl->CompositionOffset->r_FirstSampleInEntry = 0;
		stbl->CompositionOffset->max_cts_delta = 0;
	}

	if (stbl->DegradationPriority) {
		stbl->DegradationPriority->nb_entries = 0;
	}

	if (stbl->PaddingBits) {
		stbl->PaddingBits->SampleCount = 0;
	}

	if (stbl->SampleDep) {
		stbl->SampleDep->sampleCount = 0;
	}

	if (stbl->SampleSize) {
		stbl->SampleSize->sampleSize = 0;
		stbl->SampleSize->sampleCount = 0;
	}

	if (stbl->SampleToChunk) {
		stbl->SampleToChunk->nb_entries = 0;
		stbl->SampleToChunk->currentIndex = 0;
		stbl->SampleToChunk->firstSampleInCurrentChunk = 0;
		stbl->SampleToChunk->currentChunk = 0;
		stbl->SampleToChunk->ghostNumber = 0;
		stbl->SampleToChunk->w_lastSampleNumber = 0;
		stbl->SampleToChunk->w_lastChunkNumber = 0;
	}

	if (stbl->ShadowSync) {
        gf_isom_box_del_parent(&stbl->child_boxes, (GF_Box *) stbl->ShadowSync);
        stbl->ShadowSync = (GF_ShadowSyncBox *) gf_isom_box_new_parent(&stbl->child_boxes, GF_ISOM_BOX_TYPE_STSH);
	}

	if (stbl->SyncSample) {
		stbl->SyncSample->nb_entries = 0;
		stbl->SyncSample->r_LastSyncSample = 0;
		stbl->SyncSample->r_LastSampleIndex = 0;
	}

	if (stbl->TimeToSample) {
		stbl->TimeToSample->nb_entries = 0;
		stbl->TimeToSample->r_FirstSampleInEntry = 0;
		stbl->TimeToSample->r_currentEntryIndex = 0;
		stbl->TimeToSample->r_CurrentDTS = 0;
	}

	gf_isom_box_array_del_parent(&stbl->child_boxes, stbl->sai_offsets);
	stbl->sai_offsets = NULL;

	gf_isom_box_array_del_parent(&stbl->child_boxes, stbl->sai_sizes);
	stbl->sai_sizes = NULL;

	gf_isom_box_array_del_parent(&stbl->child_boxes, stbl->sampleGroups);
	stbl->sampleGroups = NULL;

	if (trak->sample_encryption) {
		if (trak->Media->information->sampleTable->child_boxes) {
			gf_list_del_item(trak->Media->information->sampleTable->child_boxes, trak->sample_encryption);
		}
		gf_isom_box_del_parent(&trak->child_boxes, (GF_Box*)trak->sample_encryption);
		trak->sample_encryption = NULL;
	}

	j = stbl->nb_sgpd_in_stbl;
	while ((a = (GF_Box *)gf_list_enum(stbl->sampleGroupsDescription, &j))) {
		gf_isom_box_del_parent(&stbl->child_boxes, a);
		j--;
		gf_list_rem(stbl->sampleGroupsDescription, j);
	}

	if (stbl->traf_map) {
		for (j=0; j<stbl->traf_map->nb_entries; j++) {
			if (stbl->traf_map->frag_starts[j].moof_template)
				gf_free(stbl->traf_map->frag_starts[j].moof_template);
		}
		memset(stbl->traf_map->frag_starts, 0, sizeof(GF_TrafMapEntry)*stbl->traf_map->nb_alloc);
		stbl->traf_map->nb_entries = 0;
	}
}


GF_EXPORT
GF_Err gf_isom_reset_tables(GF_ISOFile *movie, Bool reset_sample_count)
{
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	u32 i;

	if (!movie || !movie->moov || !movie->moov->mvex) return GF_BAD_PARAM;
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		GF_TrackBox *trak = (GF_TrackBox *)gf_list_get(movie->moov->trackList, i);

		u32 dur;
		u64 dts;
		GF_SampleTableBox *stbl = trak->Media->information->sampleTable;

		trak->sample_count_at_seg_start += stbl->SampleSize->sampleCount;
		if (trak->sample_count_at_seg_start) {
			GF_Err e;
			e = stbl_GetSampleDTS_and_Duration(stbl->TimeToSample, stbl->SampleSize->sampleCount, &dts, &dur);
			if (e == GF_OK) {
				trak->dts_at_seg_start += dts + dur;
			}
		}

		//recreate all boxes
		gf_isom_recreate_tables(trak);

#if 0
		j = stbl->nb_stbl_boxes;
		while ((a = (GF_Box *)gf_list_enum(stbl->child_boxes, &j))) {
			gf_isom_box_del_parent(&stbl->child_boxes, a);
			j--;
		}
#endif

		if (reset_sample_count) {
			trak->Media->information->sampleTable->SampleSize->sampleCount = 0;
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
			trak->sample_count_at_seg_start = 0;
			trak->dts_at_seg_start = 0;
			trak->first_traf_merged = GF_FALSE;
#endif
		}

	}
	if (reset_sample_count) {
		movie->NextMoofNumber = 0;
	}
#endif
	return GF_OK;

}

GF_EXPORT
GF_Err gf_isom_release_segment(GF_ISOFile *movie, Bool reset_tables)
{
#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	u32 i, j, base_track_sample_count;
	Bool has_scalable;
	GF_Box *a;
	if (!movie || !movie->moov || !movie->moov->mvex) return GF_BAD_PARAM;
	has_scalable = gf_isom_needs_layer_reconstruction(movie);
	base_track_sample_count = 0;
	movie->moov->compressed_diff = 0;
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(movie->moov->trackList, i);
		trak->first_traf_merged = GF_FALSE;
		if (trak->Media->information->dataHandler == movie->movieFileMap) {
			trak->Media->information->dataHandler = NULL;
		}
		if (trak->Media->information->scalableDataHandler == movie->movieFileMap) {
			trak->Media->information->scalableDataHandler = NULL;
		} else {
			if (trak->Media->information->scalableDataHandler==trak->Media->information->dataHandler)
				trak->Media->information->dataHandler = NULL;

			gf_isom_datamap_del(trak->Media->information->scalableDataHandler);
			trak->Media->information->scalableDataHandler = NULL;
		}


		if (reset_tables) {
			u32 dur;
			u64 dts;
			GF_SampleTableBox *stbl = trak->Media->information->sampleTable;

			if (has_scalable) {
				//check if the base reference is in the file - if not, do not consider the track is scalable.
				if (trak->nb_base_refs) {
					u32 on_track=0;
					GF_TrackBox *base;
					gf_isom_get_reference(movie, i+1, GF_ISOM_REF_BASE, 1, &on_track);

					base = gf_isom_get_track_from_file(movie, on_track);
					if (!base) {
						base_track_sample_count=0;
					} else {
						base_track_sample_count = base->Media->information->sampleTable->SampleSize->sampleCount;
					}
				}
			}

			trak->sample_count_at_seg_start += base_track_sample_count ? base_track_sample_count : stbl->SampleSize->sampleCount;

			if (trak->sample_count_at_seg_start) {
				GF_Err e;
				e = stbl_GetSampleDTS_and_Duration(stbl->TimeToSample, stbl->SampleSize->sampleCount, &dts, &dur);
				if (e == GF_OK) {
					trak->dts_at_seg_start += dts + dur;
				}
			}

			gf_isom_recreate_tables(trak);


#if 0 // TO CHECK
			j = ptr->nb_stbl_boxes;
			while ((a = (GF_Box *)gf_list_enum(stbl->child_boxes, &j))) {
				gf_isom_box_del_parent(&stbl->child_boxes, a);
				j--;
			}
#endif
		}


		if (movie->has_pssh_moof) {
			j = 0;
			while ((a = (GF_Box *)gf_list_enum(movie->moov->child_boxes, &j))) {
				if (a->type == GF_ISOM_BOX_TYPE_PSSH) {
					GF_ProtectionSystemHeaderBox *pssh = (GF_ProtectionSystemHeaderBox *)a;
					if (pssh->moof_defined) {
						gf_isom_box_del_parent(&movie->moov->child_boxes, a);
						j--;
					}
				}
			}
			movie->has_pssh_moof = GF_FALSE;
		}
	}

	gf_isom_datamap_del(movie->movieFileMap);
	movie->movieFileMap = NULL;

#endif
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_open_segment(GF_ISOFile *movie, const char *fileName, u64 start_range, u64 end_range, GF_ISOSegOpenMode flags)
{
#ifdef	GPAC_DISABLE_ISOM_FRAGMENTS
	return GF_NOT_SUPPORTED;
#else
	u64 MissingBytes;
	GF_Err e;
	u32 i;
	Bool segment_map_assigned = GF_FALSE;
	Bool is_scalable_segment = (flags & GF_ISOM_SEGMENT_SCALABLE_FLAG) ? GF_TRUE : GF_FALSE;
	Bool no_order_check = (flags & GF_ISOM_SEGMENT_NO_ORDER_FLAG) ? GF_TRUE: GF_FALSE;
	GF_DataMap *tmp = NULL;
	GF_DataMap *orig_file_map = NULL;
	if (!movie || !movie->moov || !movie->moov->mvex) return GF_BAD_PARAM;
	if (movie->openMode != GF_ISOM_OPEN_READ) return GF_BAD_PARAM;

	/*this is a scalable segment - use a temp data map for the associated track(s) but do NOT touch the movie file map*/
	if (is_scalable_segment) {
		tmp = NULL;
		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_READ_ONLY, &tmp);
		if (e) return e;

		orig_file_map = movie->movieFileMap;
		movie->movieFileMap = tmp;
	} else {
		if (movie->movieFileMap)
			gf_isom_release_segment(movie, GF_FALSE);

		e = gf_isom_datamap_new(fileName, NULL, GF_ISOM_DATA_MAP_READ_ONLY, &movie->movieFileMap);
		if (e) return e;
	}
	movie->moov->compressed_diff = 0;
	movie->current_top_box_start = 0;

	if (start_range || end_range) {
		if (end_range > start_range) {
			gf_bs_seek(movie->movieFileMap->bs, end_range+1);
			gf_bs_truncate(movie->movieFileMap->bs);
		}
		gf_bs_seek(movie->movieFileMap->bs, start_range);
		movie->current_top_box_start = start_range;
	}

	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(movie->moov->trackList, i);

		if (!is_scalable_segment) {
			/*reset data handler to new segment*/
			if (trak->Media->information->dataHandler == NULL) {
				trak->Media->information->dataHandler = movie->movieFileMap;
			}
		} else {
			trak->present_in_scalable_segment = GF_FALSE;
		}
	}
	if (no_order_check) movie->NextMoofNumber = 0;

	//ok parse root boxes
	e = gf_isom_parse_movie_boxes(movie, NULL, &MissingBytes, GF_TRUE);

	if (!is_scalable_segment)
		return e;

	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(movie->moov->trackList, i);
		if (trak->present_in_scalable_segment) {
			/*store the temp dataHandler into scalableDataHandler so that it will not be destroyed
			if we append another representation - destruction of this data handler is done in release_segment*/
			trak->Media->information->scalableDataHandler = tmp;
			if (!segment_map_assigned) {
				trak->Media->information->scalableDataHandler = tmp;
				segment_map_assigned = GF_TRUE;
			}
			//and update the regular dataHandler for the Media_GetSample function
			trak->Media->information->dataHandler = tmp;
		}
	}
	movie->movieFileMap = 	orig_file_map;
	return e;
#endif
}

GF_EXPORT
GF_ISOTrackID gf_isom_get_highest_track_in_scalable_segment(GF_ISOFile *movie, u32 for_base_track)
{
#ifdef	GPAC_DISABLE_ISOM_FRAGMENTS
	return 0;
#else
	s32 max_ref;
	u32 i, j;
	GF_ISOTrackID track_id;

	max_ref = 0;
	track_id = 0;
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		s32 ref;
		u32 ref_type = GF_ISOM_REF_SCAL;
		GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(movie->moov->trackList, i);
		if (! trak->present_in_scalable_segment) continue;

		ref = gf_isom_get_reference_count(movie, i+1, ref_type);
		if (ref<=0) {
			//handle implicit reconstruction for LHE1/LHV1, check sbas track ref
			u32 subtype = gf_isom_get_media_subtype(movie, i+1, 1);
			switch (subtype) {
			case GF_ISOM_SUBTYPE_LHE1:
			case GF_ISOM_SUBTYPE_LHV1:
				ref = gf_isom_get_reference_count(movie, i+1, GF_ISOM_REF_BASE);
				if (ref<=0) continue;
				break;
			default:
				continue;
			}
		}
		if (ref<=max_ref) continue;

		for (j=0; j< (u32) ref; j++) {
			u32 on_track=0;
			gf_isom_get_reference(movie, i+1, GF_ISOM_REF_BASE, j+1, &on_track);
			if (on_track==for_base_track) {
				max_ref = ref;
				track_id = trak->Header->trackID;
			}
		}
	}
	return track_id;
#endif
}


GF_EXPORT
GF_Err gf_isom_text_set_streaming_mode(GF_ISOFile *movie, Bool do_convert)
{
	if (!movie) return GF_BAD_PARAM;
	movie->convert_streaming_text = do_convert;
	return GF_OK;
}

static void gf_isom_gen_desc_get_dsi(GF_GenericSampleDescription *udesc, GF_List *child_boxes)
{
	if (!child_boxes) return;
	GF_UnknownBox *a=NULL;
	u32 i=0;
	while ((a=gf_list_enum(child_boxes, &i))) {
		if (a->type == GF_ISOM_BOX_TYPE_UNKNOWN) break;
		a = NULL;
	}
	if (!a) return;
	udesc->extension_buf = (char*)gf_malloc(sizeof(char) * a->dataSize);
	if (udesc->extension_buf) {
		udesc->extension_buf_size = a->dataSize;
		memcpy(udesc->extension_buf, a->data, a->dataSize);
		udesc->ext_box_wrap = a->original_4cc;
	}
}

GF_EXPORT
GF_GenericSampleDescription *gf_isom_get_generic_sample_description(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_GenericVisualSampleEntryBox *entry;
	GF_GenericAudioSampleEntryBox *gena;
	GF_GenericSampleEntryBox *genm;
	GF_TrackBox *trak;
	GF_GenericSampleDescription *udesc;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !StreamDescriptionIndex || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable) return 0;

	entry = (GF_GenericVisualSampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, StreamDescriptionIndex-1);
	//no entry or MPEG entry:
	if (!entry || IsMP4Description(entry->type) ) return NULL;
	//if we handle the description return false
	switch (entry->type) {
	case GF_ISOM_SUBTYPE_3GP_AMR:
	case GF_ISOM_SUBTYPE_3GP_AMR_WB:
	case GF_ISOM_SUBTYPE_3GP_EVRC:
	case GF_ISOM_SUBTYPE_3GP_QCELP:
	case GF_ISOM_SUBTYPE_3GP_SMV:
	case GF_ISOM_SUBTYPE_3GP_H263:
		return NULL;
	case GF_ISOM_BOX_TYPE_GNRV:
		GF_SAFEALLOC(udesc, GF_GenericSampleDescription);
		if (!udesc) return NULL;
		if (entry->EntryType == GF_ISOM_BOX_TYPE_UUID) {
			memcpy(udesc->UUID, ((GF_UUIDBox*)entry)->uuid, sizeof(bin128));
		} else {
			udesc->codec_tag = entry->EntryType;
		}
		udesc->version = entry->version;
		udesc->revision = entry->revision;
		udesc->vendor_code = entry->vendor;
		udesc->temporal_quality = entry->temporal_quality;
		udesc->spatial_quality = entry->spatial_quality;
		udesc->width = entry->Width;
		udesc->height = entry->Height;
		udesc->h_res = entry->horiz_res;
		udesc->v_res = entry->vert_res;
		strcpy(udesc->compressor_name, entry->compressor_name);
		udesc->depth = entry->bit_depth;
		udesc->color_table_index = entry->color_table_index;
		if (entry->data_size) {
			udesc->extension_buf_size = entry->data_size;
			udesc->extension_buf = (char*)gf_malloc(sizeof(char) * entry->data_size);
			if (!udesc->extension_buf) {
				gf_free(udesc);
				return NULL;
			}
			memcpy(udesc->extension_buf, entry->data, entry->data_size);
		} else {
			gf_isom_gen_desc_get_dsi(udesc, entry->child_boxes);
		}
		return udesc;
	case GF_ISOM_BOX_TYPE_GNRA:
		gena = (GF_GenericAudioSampleEntryBox *)entry;
		GF_SAFEALLOC(udesc, GF_GenericSampleDescription);
		if (!udesc) return NULL;
		if (gena->EntryType == GF_ISOM_BOX_TYPE_UUID) {
			memcpy(udesc->UUID, ((GF_UUIDBox*)gena)->uuid, sizeof(bin128));
		} else {
			udesc->codec_tag = gena->EntryType;
		}
		udesc->version = gena->version;
		udesc->revision = gena->revision;
		udesc->vendor_code = gena->vendor;
		udesc->samplerate = gena->samplerate_hi;
		udesc->bits_per_sample = gena->bitspersample;
		udesc->nb_channels = gena->channel_count;
		if (gena->data_size) {
			udesc->extension_buf_size = gena->data_size;
			udesc->extension_buf = (char*)gf_malloc(sizeof(char) * gena->data_size);
			if (!udesc->extension_buf) {
				gf_free(udesc);
				return NULL;
			}
			memcpy(udesc->extension_buf, gena->data, gena->data_size);
		} else {
			gf_isom_gen_desc_get_dsi(udesc, entry->child_boxes);
		}
		return udesc;
	case GF_ISOM_BOX_TYPE_GNRM:
		genm = (GF_GenericSampleEntryBox *)entry;
		GF_SAFEALLOC(udesc, GF_GenericSampleDescription);
		if (!udesc) return NULL;
		if (genm->EntryType == GF_ISOM_BOX_TYPE_UUID) {
			memcpy(udesc->UUID, ((GF_UUIDBox*)genm)->uuid, sizeof(bin128));
		} else {
			udesc->codec_tag = genm->EntryType;
		}
		if (genm->data_size) {
			udesc->extension_buf_size = genm->data_size;
			udesc->extension_buf = (char*)gf_malloc(sizeof(char) * genm->data_size);
			if (!udesc->extension_buf) {
				gf_free(udesc);
				return NULL;
			}
			memcpy(udesc->extension_buf, genm->data, genm->data_size);
		} else {
			gf_isom_gen_desc_get_dsi(udesc, entry->child_boxes);
		}
		return udesc;
	}
	return NULL;
}

GF_EXPORT
GF_Err gf_isom_get_visual_info(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 *Width, u32 *Height)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) return movie->LastError = GF_BAD_PARAM;

	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;

	//valid for MPEG visual, JPG and 3GPP H263
	if (entry->internal_type == GF_ISOM_SAMPLE_ENTRY_VIDEO) {
		*Width = ((GF_VisualSampleEntryBox*)entry)->Width;
		*Height = ((GF_VisualSampleEntryBox*)entry)->Height;
	} else if (trak->Media->handler->handlerType==GF_ISOM_MEDIA_SCENE) {
		*Width = trak->Header->width>>16;
		*Height = trak->Header->height>>16;
	} else {
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_visual_bit_depth(GF_ISOFile* movie, u32 trackNumber, u32 StreamDescriptionIndex, u16* bitDepth)
{
	GF_TrackBox* trak;
	GF_SampleEntryBox* entry;
	GF_SampleDescriptionBox* stsd;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) return movie->LastError = GF_BAD_PARAM;

	entry = (GF_SampleEntryBox*)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);

	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;

	//valid for MPEG visual, JPG and 3GPP H263
	if (entry->internal_type == GF_ISOM_SAMPLE_ENTRY_VIDEO) {
		*bitDepth = ((GF_VisualSampleEntryBox*)entry)->bit_depth;
	} else {
		return GF_BAD_PARAM;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_audio_info(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 *SampleRate, u32 *Channels, u32 *bitsPerSample)
{
	GF_TrackBox *trak;
	GF_AudioSampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd = NULL;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	if (trak->Media && trak->Media->information && trak->Media->information->sampleTable && trak->Media->information->sampleTable->SampleDescription)
		stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) return movie->LastError = GF_BAD_PARAM;

	entry = (GF_AudioSampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return GF_BAD_PARAM;

	if (SampleRate) {
		(*SampleRate) = entry->samplerate_hi;
		if (entry->type==GF_ISOM_BOX_TYPE_MLPA) {
			u32 sr = entry->samplerate_hi;
			sr <<= 16;
			sr |= entry->samplerate_lo;
			(*SampleRate) = sr;
		}
	}
	if (Channels) (*Channels) = entry->channel_count;
	if (bitsPerSample) (*bitsPerSample) = (u8) entry->bitspersample;

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_audio_layout(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, GF_AudioChannelLayout *layout)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;
	GF_ChannelLayoutBox *chnl;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !layout) return GF_BAD_PARAM;
	memset(layout, 0, sizeof(GF_AudioChannelLayout));

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) return movie->LastError = GF_BAD_PARAM;

	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_BAD_PARAM;

	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_AUDIO) return GF_BAD_PARAM;
	chnl = (GF_ChannelLayoutBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_CHNL);
	if (!chnl) return GF_NOT_FOUND;

	memcpy(layout, &chnl->layout, sizeof(GF_AudioChannelLayout));
	return GF_OK;
}
GF_EXPORT
GF_Err gf_isom_get_pixel_aspect_ratio(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 *hSpacing, u32 *vSpacing)
{
	GF_TrackBox *trak;
	GF_VisualSampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !hSpacing || !vSpacing) return GF_BAD_PARAM;
	*hSpacing = 1;
	*vSpacing = 1;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) return movie->LastError = GF_BAD_PARAM;

	entry = (GF_VisualSampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_OK;

	//valid for MPEG visual, JPG and 3GPP H263
	if (entry->internal_type==GF_ISOM_SAMPLE_ENTRY_VIDEO) {
		GF_PixelAspectRatioBox *pasp = (GF_PixelAspectRatioBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_PASP);
		if (pasp) {
			*hSpacing = pasp->hSpacing;
			*vSpacing = pasp->vSpacing;
		}
		return GF_OK;
	} else {
		return GF_BAD_PARAM;
	}
}

GF_EXPORT
GF_Err gf_isom_get_color_info(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 *colour_type, u16 *colour_primaries, u16 *transfer_characteristics, u16 *matrix_coefficients, Bool *full_range_flag)
{
	GF_TrackBox *trak;
	GF_VisualSampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) return movie->LastError = GF_BAD_PARAM;

	entry = (GF_VisualSampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_OK;

	//valid for MPEG visual, JPG and 3GPP H263
	if (entry->internal_type!=GF_ISOM_SAMPLE_ENTRY_VIDEO) {
		return GF_BAD_PARAM;
	}

	u32 i, count = gf_list_count(entry->child_boxes);
	for (i=0; i<count; i++) {
		GF_ColourInformationBox *clr = (GF_ColourInformationBox *) gf_list_get(entry->child_boxes, i);
		if (clr->type != GF_ISOM_BOX_TYPE_COLR) continue;
		if (clr->is_jp2) continue;
		if (clr->opaque_size) continue;

		if (colour_type) *colour_type = clr->colour_type;
		if (colour_primaries) *colour_primaries = clr->colour_primaries;
		if (transfer_characteristics) *transfer_characteristics = clr->transfer_characteristics;
		if (matrix_coefficients) *matrix_coefficients = clr->matrix_coefficients;
		if (full_range_flag) *full_range_flag = clr->full_range_flag;
		return GF_OK;
	}
	return GF_NOT_FOUND;
}

GF_EXPORT
GF_Err gf_isom_get_icc_profile(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, Bool *icc_restricted, const u8 **icc, u32 *icc_size)
{
	GF_TrackBox *trak;
	GF_VisualSampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

	if (!icc || !icc_size) return GF_BAD_PARAM;
	*icc = NULL;
	*icc_size = 0;
	if (icc_restricted) *icc_restricted = GF_FALSE;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) return movie->LastError = GF_BAD_PARAM;

	entry = (GF_VisualSampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (entry == NULL) return GF_OK;

	//valid for MPEG visual, JPG and 3GPP H263
	if (entry->internal_type!=GF_ISOM_SAMPLE_ENTRY_VIDEO) {
		return GF_BAD_PARAM;
	}

	u32 i, count = gf_list_count(entry->child_boxes);
	for (i=0; i<count; i++) {
		GF_ColourInformationBox *clr = (GF_ColourInformationBox *) gf_list_get(entry->child_boxes, i);
		if (clr->type != GF_ISOM_BOX_TYPE_COLR) continue;
		if (clr->is_jp2) continue;
		if (!clr->opaque_size) continue;

		if (clr->colour_type==GF_4CC('r', 'I', 'C', 'C')) {
			if (icc_restricted) *icc_restricted = GF_TRUE;
			*icc = clr->opaque;
			*icc_size = clr->opaque_size;
		}
		else if (clr->colour_type==GF_4CC('p', 'r', 'o', 'f')) {
			*icc = clr->opaque;
			*icc_size = clr->opaque_size;
		}
		return GF_OK;
	}
	return GF_NOT_FOUND;
}

GF_EXPORT
const char *gf_isom_get_filename(GF_ISOFile *movie)
{
	if (!movie) return NULL;
#ifndef GPAC_DISABLE_ISOM_WRITE
	if (movie->finalName && !movie->fileName) return movie->finalName;
#endif
	return movie->fileName;
}


GF_EXPORT
u8 gf_isom_get_pl_indication(GF_ISOFile *movie, GF_ISOProfileLevelType PL_Code)
{
	GF_IsomInitialObjectDescriptor *iod;
	if (!movie || !movie->moov) return 0xFF;
	if (!movie->moov->iods || !movie->moov->iods->descriptor) return 0xFF;
	if (movie->moov->iods->descriptor->tag != GF_ODF_ISOM_IOD_TAG) return 0xFF;

	iod = (GF_IsomInitialObjectDescriptor *)movie->moov->iods->descriptor;
	switch (PL_Code) {
	case GF_ISOM_PL_AUDIO:
		return iod->audio_profileAndLevel;
	case GF_ISOM_PL_VISUAL:
		return iod->visual_profileAndLevel;
	case GF_ISOM_PL_GRAPHICS:
		return iod->graphics_profileAndLevel;
	case GF_ISOM_PL_SCENE:
		return iod->scene_profileAndLevel;
	case GF_ISOM_PL_OD:
		return iod->OD_profileAndLevel;
	case GF_ISOM_PL_INLINE:
		return iod->inlineProfileFlag;
	case GF_ISOM_PL_MPEGJ:
	default:
		return 0xFF;
	}
}

GF_EXPORT
GF_Err gf_isom_get_track_matrix(GF_ISOFile *the_file, u32 trackNumber, u32 matrix[9])
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak || !trak->Header) return GF_BAD_PARAM;
	memcpy(matrix, trak->Header->matrix, sizeof(trak->Header->matrix));
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_track_layout_info(GF_ISOFile *movie, u32 trackNumber, u32 *width, u32 *height, s32 *translation_x, s32 *translation_y, s16 *layer)
{
	GF_TrackBox *tk = gf_isom_get_track_from_file(movie, trackNumber);
	if (!tk) return GF_BAD_PARAM;
	if (width) *width = tk->Header->width>>16;
	if (height) *height = tk->Header->height>>16;
	if (layer) *layer = tk->Header->layer;
	if (translation_x) *translation_x = tk->Header->matrix[6] >> 16;
	if (translation_y) *translation_y = tk->Header->matrix[7] >> 16;
	return GF_OK;
}


/*returns total amount of media bytes in track*/
GF_EXPORT
u64 gf_isom_get_media_data_size(GF_ISOFile *movie, u32 trackNumber)
{
	u32 i;
	u64 size;
	GF_SampleSizeBox *stsz;
	GF_TrackBox *tk = gf_isom_get_track_from_file(movie, trackNumber);
	if (!tk) return 0;
	stsz = tk->Media->information->sampleTable->SampleSize;
	if (!stsz) return 0;
	if (stsz->sampleSize) return stsz->sampleSize*stsz->sampleCount;
	size = 0;
	for (i=0; i<stsz->sampleCount; i++) size += stsz->sizes[i];
	return size;
}

GF_EXPORT
u64 gf_isom_get_first_mdat_start(GF_ISOFile *movie)
{
	u64 offset;
	if (!movie) return 0;
	offset = movie->first_data_toplevel_offset + 8;
	if (movie->first_data_toplevel_size > 0xFFFFFFFFUL)
		offset += 8;
	return offset;
}

static u64 box_unused_bytes(GF_Box *b)
{
	u32 i, count;
	u64 size = 0;
	switch (b->type) {
	case GF_QT_BOX_TYPE_WIDE:
	case GF_ISOM_BOX_TYPE_FREE:
	case GF_ISOM_BOX_TYPE_SKIP:
		size += b->size;
		break;
	}
	count = gf_list_count(b->child_boxes);
	for (i=0; i<count; i++) {
		GF_Box *child = gf_list_get(b->child_boxes, i);
		size += box_unused_bytes(child);
	}
	return size;
}

extern u64 unused_bytes;

GF_EXPORT
u64 gf_isom_get_unused_box_bytes(GF_ISOFile *movie)
{
	u64 size = unused_bytes;
	u32 i, count;
	if (!movie) return 0;
	count = gf_list_count(movie->TopBoxes);
	for (i=0; i<count; i++) {
		GF_Box *b = gf_list_get(movie->TopBoxes, i);
		size += box_unused_bytes(b);
	}
	return size;
}

GF_EXPORT
void gf_isom_set_default_sync_track(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *tk = gf_isom_get_track_from_file(movie, trackNumber);
	if (!tk) movie->es_id_default_sync = -1;
	else movie->es_id_default_sync = tk->Header->trackID;
}


GF_EXPORT
Bool gf_isom_is_single_av(GF_ISOFile *file)
{
	u32 count, i, nb_any, nb_a, nb_v, nb_auxv, nb_pict, nb_scene, nb_od, nb_text;
	nb_auxv = nb_pict = nb_a = nb_v = nb_any = nb_scene = nb_od = nb_text = 0;

	if (!file->moov) return GF_FALSE;
	count = gf_isom_get_track_count(file);
	for (i=0; i<count; i++) {
		u32 mtype = gf_isom_get_media_type(file, i+1);
		switch (mtype) {
		case GF_ISOM_MEDIA_SCENE:
			if (gf_isom_get_sample_count(file, i+1)>1) nb_any++;
			else nb_scene++;
			break;
		case GF_ISOM_MEDIA_OD:
			if (gf_isom_get_sample_count(file, i+1)>1) nb_any++;
			else nb_od++;
			break;
		case GF_ISOM_MEDIA_TEXT:
		case GF_ISOM_MEDIA_SUBT:
			nb_text++;
			break;
		case GF_ISOM_MEDIA_AUDIO:
			nb_a++;
			break;
        case GF_ISOM_MEDIA_AUXV:
            /*discard file with images*/
            if (gf_isom_get_sample_count(file, i+1)==1) nb_any++;
            else nb_auxv++;
            break;
        case GF_ISOM_MEDIA_PICT:
            /*discard file with images*/
            if (gf_isom_get_sample_count(file, i+1)==1) nb_any++;
            else nb_pict++;
            break;
		case GF_ISOM_MEDIA_VISUAL:
			/*discard file with images*/
			if (gf_isom_get_sample_count(file, i+1)==1) nb_any++;
			else nb_v++;
			break;
		default:
			nb_any++;
			break;
		}
	}
	if (nb_any) return GF_FALSE;
	if ((nb_scene<=1) && (nb_od<=1) && (nb_a<=1) && (nb_v+nb_pict+nb_auxv<=1) && (nb_text<=1) ) return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
Bool gf_isom_is_JPEG2000(GF_ISOFile *mov)
{
	return (mov && mov->is_jp2) ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
u32 gf_isom_guess_specification(GF_ISOFile *file)
{
	u32 count, i, nb_any, nb_m4s, nb_a, nb_v, nb_auxv,nb_scene, nb_od, nb_mp3, nb_aac, nb_m4v, nb_avc, nb_amr, nb_h263, nb_qcelp, nb_evrc, nb_smv, nb_text, nb_pict;

	nb_m4s = nb_a = nb_v = nb_auxv = nb_any = nb_scene = nb_od = nb_mp3 = nb_aac = nb_m4v = nb_avc = nb_amr = nb_h263 = nb_qcelp = nb_evrc = nb_smv = nb_text = nb_pict = 0;

	if (file->is_jp2) {
		if (file->moov) return GF_ISOM_BRAND_MJP2;
		return GF_ISOM_BRAND_JP2;
	}
	if (!file->moov) {
		if (!file->meta || !file->meta->handler) return 0;
		return file->meta->handler->handlerType;
	}

	count = gf_isom_get_track_count(file);
	for (i=0; i<count; i++) {
		u32 mtype = gf_isom_get_media_type(file, i+1);
		u32 mstype = gf_isom_get_media_subtype(file, i+1, 1);

		if (mtype==GF_ISOM_MEDIA_SCENE) {
			nb_scene++;
			/*forces non-isma*/
			if (gf_isom_get_sample_count(file, i+1)>1) nb_m4s++;
		} else if (mtype==GF_ISOM_MEDIA_OD) {
			nb_od++;
			/*forces non-isma*/
			if (gf_isom_get_sample_count(file, i+1)>1) nb_m4s++;
		}
		else if ((mtype==GF_ISOM_MEDIA_TEXT) || (mtype==GF_ISOM_MEDIA_SUBT)) nb_text++;
		else if ((mtype==GF_ISOM_MEDIA_AUDIO) || gf_isom_is_video_handler_type(mtype) ) {
			switch (mstype) {
			case GF_ISOM_SUBTYPE_3GP_AMR:
			case GF_ISOM_SUBTYPE_3GP_AMR_WB:
				nb_amr++;
				break;
			case GF_ISOM_SUBTYPE_3GP_H263:
				nb_h263++;
				break;
			case GF_ISOM_SUBTYPE_3GP_EVRC:
				nb_evrc++;
				break;
			case GF_ISOM_SUBTYPE_3GP_QCELP:
				nb_qcelp++;
				break;
			case GF_ISOM_SUBTYPE_3GP_SMV:
				nb_smv++;
				break;
			case GF_ISOM_SUBTYPE_AVC_H264:
			case GF_ISOM_SUBTYPE_AVC2_H264:
			case GF_ISOM_SUBTYPE_AVC3_H264:
			case GF_ISOM_SUBTYPE_AVC4_H264:
				nb_avc++;
				break;
			case GF_ISOM_SUBTYPE_SVC_H264:
			case GF_ISOM_SUBTYPE_MVC_H264:
				nb_avc++;
				break;
			case GF_ISOM_SUBTYPE_MPEG4:
			case GF_ISOM_SUBTYPE_MPEG4_CRYP:
			{
				GF_DecoderConfig *dcd = gf_isom_get_decoder_config(file, i+1, 1);
				if (!dcd) break;
				switch (dcd->streamType) {
				case GF_STREAM_VISUAL:
					if (dcd->objectTypeIndication==GF_CODECID_MPEG4_PART2) nb_m4v++;
					else if ((dcd->objectTypeIndication==GF_CODECID_AVC) || (dcd->objectTypeIndication==GF_CODECID_SVC) || (dcd->objectTypeIndication==GF_CODECID_MVC)) nb_avc++;
					else nb_v++;
					break;
				case GF_STREAM_AUDIO:
					switch (dcd->objectTypeIndication) {
					case GF_CODECID_AAC_MPEG2_MP:
					case GF_CODECID_AAC_MPEG2_LCP:
					case GF_CODECID_AAC_MPEG2_SSRP:
					case GF_CODECID_AAC_MPEG4:
						nb_aac++;
						break;
					case GF_CODECID_MPEG2_PART3:
					case GF_CODECID_MPEG_AUDIO:
					case GF_CODECID_MPEG_AUDIO_L1:
						nb_mp3++;
						break;
					case GF_CODECID_EVRC:
						nb_evrc++;
						break;
					case GF_CODECID_SMV:
						nb_smv++;
						break;
					case GF_CODECID_QCELP:
						nb_qcelp++;
						break;
					default:
						nb_a++;
						break;
					}
					break;
				/*SHOULD NEVER HAPPEN - IF SO, BROKEN MPEG4 FILE*/
				default:
					nb_any++;
					break;
				}
				gf_odf_desc_del((GF_Descriptor *)dcd);
			}
				break;
			default:
				if (mtype==GF_ISOM_MEDIA_VISUAL) nb_v++;
				else if (mtype==GF_ISOM_MEDIA_AUXV) nb_auxv++;
				else if (mtype==GF_ISOM_MEDIA_PICT) nb_pict++;
				else nb_a++;
				break;
			}
		} else if ((mtype==GF_ISOM_SUBTYPE_MPEG4) || (mtype==GF_ISOM_SUBTYPE_MPEG4_CRYP)) nb_m4s++;
		else nb_any++;
	}
	if (nb_any) return GF_ISOM_BRAND_ISOM;
	if (nb_qcelp || nb_evrc || nb_smv) {
		/*non std mix of streams*/
		if (nb_m4s || nb_avc || nb_scene || nb_od || nb_mp3 || nb_a || nb_v) return GF_ISOM_BRAND_ISOM;
		return GF_ISOM_BRAND_3G2A;
	}
	/*other a/v/s streams*/
	if (nb_v || nb_a || nb_m4s) return GF_ISOM_BRAND_MP42;

	nb_v = nb_m4v + nb_avc + nb_h263;
	nb_a = nb_mp3 + nb_aac + nb_amr;

	/*avc file: whatever has AVC and no systems*/
	if (nb_avc) {
		if (!nb_scene && !nb_od) return GF_ISOM_BRAND_AVC1;
		return GF_ISOM_BRAND_MP42;
	}
	/*MP3: ISMA and MPEG4*/
	if (nb_mp3) {
		if (!nb_text && (nb_v<=1) && (nb_a<=1) && (nb_scene==1) && (nb_od==1))
			return GF_ISOM_BRAND_ISMA;
		return GF_ISOM_BRAND_MP42;
	}
	/*MP4*/
	if (nb_scene || nb_od) {
		/*issue with AMR and H263 which don't have MPEG mapping: non compliant file*/
		if (nb_amr || nb_h263) return GF_ISOM_BRAND_ISOM;
		return GF_ISOM_BRAND_MP42;
	}
	/*use ISMA (3GP fine too)*/
	if (!nb_amr && !nb_h263 && !nb_text) {
		if ((nb_v<=1) && (nb_a<=1)) return GF_ISOM_BRAND_ISMA;
		return GF_ISOM_BRAND_MP42;
	}

	if ((nb_v<=1) && (nb_a<=1) && (nb_text<=1)) return nb_text ? GF_ISOM_BRAND_3GP6 : GF_ISOM_BRAND_3GP5;
	return GF_ISOM_BRAND_3GG6;
}

GF_ItemListBox *gf_isom_locate_box(GF_List *list, u32 boxType, bin128 UUID)
{
	u32 i;
	GF_Box *box;
	i=0;
	while ((box = (GF_Box *)gf_list_enum(list, &i))) {
		if (box->type == boxType) {
			GF_UUIDBox* box2 = (GF_UUIDBox* )box;
			if (boxType != GF_ISOM_BOX_TYPE_UUID) return (GF_ItemListBox *)box;
			if (!memcmp(box2->uuid, UUID, 16)) return (GF_ItemListBox *)box;
		}
	}
	return NULL;
}

/*Apple extensions*/


GF_EXPORT
GF_Err gf_isom_apple_get_tag(GF_ISOFile *mov, GF_ISOiTunesTag tag, const u8 **data, u32 *data_len)
{
	u32 i;
	GF_ListItemBox *info;
	GF_ItemListBox *ilst;
	GF_MetaBox *meta;

	*data = NULL;
	*data_len = 0;

	meta = (GF_MetaBox *) gf_isom_get_meta_extensions(mov, 0);
	if (!meta) return GF_URL_ERROR;

	ilst = gf_isom_locate_box(meta->child_boxes, GF_ISOM_BOX_TYPE_ILST, NULL);
	if (!ilst) return GF_URL_ERROR;

	if (tag==GF_ISOM_ITUNE_PROBE) return gf_list_count(ilst->child_boxes) ? GF_OK : GF_URL_ERROR;

	i=0;
	while ( (info=(GF_ListItemBox*)gf_list_enum(ilst->child_boxes, &i))) {
		if (info->type==tag) break;
		/*special cases*/
		if ((tag==GF_ISOM_ITUNE_GENRE) && (info->type==(u32) GF_ISOM_ITUNE_GENRE_USER)) break;
		info = NULL;
	}
	if (!info || !info->data || !info->data->data) return GF_URL_ERROR;

	if ((tag == GF_ISOM_ITUNE_GENRE) && (info->data->flags == 0)) {
		if (info->data->dataSize && (info->data->dataSize>2) && (info->data->dataSize < 5)) {
			GF_BitStream* bs = gf_bs_new(info->data->data, info->data->dataSize, GF_BITSTREAM_READ);
			*data_len = gf_bs_read_int(bs, info->data->dataSize * 8);
			gf_bs_del(bs);
			return GF_OK;
		}
	}
//	if (info->data->flags != 0x1) return GF_URL_ERROR;
	*data = info->data->data;
	*data_len = info->data->dataSize;
	if ((tag==GF_ISOM_ITUNE_COVER_ART) && (info->data->flags==14)) *data_len |= 0x80000000; //(1<<31);
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_apple_enum_tag(GF_ISOFile *mov, u32 idx, GF_ISOiTunesTag *out_tag, const u8 **data, u32 *data_len, u64 *out_int_val, u32 *out_int_val2, u32 *out_flags)
{
	u32 i, child_index;
	GF_ListItemBox *info;
	GF_ItemListBox *ilst;
	GF_MetaBox *meta;
	GF_DataBox *dbox = NULL;
	Bool found=GF_FALSE;
	u32 itype, tag_val;
	s32 tag_idx;
	*data = NULL;
	*data_len = 0;
	*out_int_val = 0;
	*out_int_val2 = 0;
	*out_flags = 0;

	meta = (GF_MetaBox *) gf_isom_get_meta_extensions(mov, 0);
	if (!meta) return GF_URL_ERROR;

	ilst = gf_isom_locate_box(meta->child_boxes, GF_ISOM_BOX_TYPE_ILST, NULL);
	if (!ilst) return GF_URL_ERROR;

	child_index = i = 0;
	while ( (info=(GF_ListItemBox*)gf_list_enum(ilst->child_boxes, &i))) {
		GF_DataBox *data_box = NULL;
		if (gf_itags_find_by_itag(info->type)<0) {
			tag_val = info->type;
			if (info->type==GF_ISOM_BOX_TYPE_UNKNOWN) {
				data_box = (GF_DataBox *) gf_isom_box_find_child(info->child_boxes, GF_ISOM_BOX_TYPE_DATA);
				if (!data_box) continue;
				tag_val = ((GF_UnknownBox *)info)->original_4cc;
			}
		} else {
			data_box = info->data;
			tag_val = info->type;
		}
		if (child_index==idx) {
			dbox = data_box;
			found = GF_TRUE;
			break;
		}
		child_index++;
	}

	if (!dbox) {
		if (found) {
			*data = NULL;
			*data_len = 1;
			*out_tag = tag_val;
			return GF_OK;
		}
		return GF_URL_ERROR;
	}
	*out_flags = dbox->flags;
	*out_tag = tag_val;
	if (!dbox->data) {
		*data = NULL;
		*data_len = 1;
		return GF_OK;
	}

	tag_idx = gf_itags_find_by_itag(info->type);
	if (tag_idx<0) {
		*data = dbox->data;
		*data_len = dbox->dataSize;
		return GF_OK;
	}

	if ((tag_val == GF_ISOM_ITUNE_GENRE) && (dbox->flags == 0) && (dbox->dataSize>=2)) {
		u32 int_val = dbox->data[0];
		int_val <<= 8;
		int_val |= dbox->data[1];
		*data = NULL;
		*data_len = 0;
		*out_int_val = int_val;
		return GF_OK;
	}

	itype = gf_itags_get_type((u32) tag_idx);
	switch (itype) {
	case GF_ITAG_BOOL:
	case GF_ITAG_INT8:
		if (dbox->dataSize) *out_int_val = dbox->data[0];
		break;
	case GF_ITAG_INT16:
		if (dbox->dataSize>1) {
			u16 v = dbox->data[0];
			v<<=8;
			v |= dbox->data[1];
			*out_int_val = v;
		}
		break;
	case GF_ITAG_INT32:
		if (dbox->dataSize>3) {
			u32 v = dbox->data[0];
			v<<=8;
			v |= dbox->data[1];
			v<<=8;
			v |= dbox->data[2];
			v<<=8;
			v |= dbox->data[3];
			*out_int_val = v;
		}
		break;
	case GF_ITAG_INT64:
		if (dbox->dataSize>7) {
			u64 v = dbox->data[0];
			v<<=8;
			v |= dbox->data[1];
			v<<=8;
			v |= dbox->data[2];
			v<<=8;
			v |= dbox->data[3];
			v<<=8;
			v |= dbox->data[4];
			v<<=8;
			v |= dbox->data[5];
			v<<=8;
			v |= dbox->data[6];
			v<<=8;
			v |= dbox->data[7];
			*out_int_val = v;
		}
		break;
	case GF_ITAG_FRAC6:
	case GF_ITAG_FRAC8:
		if (dbox->dataSize>5) {
			u32 v = dbox->data[2];
			v<<=8;
			v |= dbox->data[3];
			*out_int_val = v;
			v = dbox->data[4];
			v<<=8;
			v |= dbox->data[5];
			*out_int_val2 = v;
		}
		break;
	default:
		*data = dbox->data;
		*data_len = dbox->dataSize;
		break;
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_enum_udta_keys(GF_ISOFile *mov, u32 idx, GF_QT_UDTAKey *okey)
{
	u32 i, count;

	GF_MetaBox *meta = (GF_MetaBox *) gf_isom_get_meta_extensions(mov, 2);
	if (!meta || !meta->keys) return GF_URL_ERROR;

	GF_MetaKey *k = gf_list_get(meta->keys->keys, idx);
	if (!k) return GF_URL_ERROR;
	if (!okey) return GF_OK;

	memset(okey, 0, sizeof(GF_QT_UDTAKey) );
	okey->name = k->data;
	okey->ns = k->ns;

	GF_ListItemBox *ilst = (GF_ListItemBox *) gf_isom_locate_box(meta->child_boxes, GF_ISOM_BOX_TYPE_ILST, NULL);
	if (!ilst) return GF_OK;

	GF_DataBox *data_box = NULL;
	count = gf_list_count(ilst->child_boxes);
	for (i=0; i<count; i++) {
		GF_UnknownBox *u = gf_list_get(ilst->child_boxes, i);
		if (u->type!=GF_ISOM_BOX_TYPE_UNKNOWN) continue;
		if (u->original_4cc==idx+1) {
			data_box = (GF_DataBox *) gf_isom_box_find_child(u->child_boxes, GF_ISOM_BOX_TYPE_DATA);
		}
	}

	okey->type=GF_QT_KEY_OPAQUE;
	if (!data_box || (data_box->version!=0)) {
		if (data_box) {
			okey->value.data.data = data_box->data;
			okey->value.data.data_len = data_box->dataSize;
		}
		return GF_OK;
	}
	okey->type = data_box->flags;

	u32 nb_bits = 8 * data_box->dataSize;
	GF_BitStream *bs = gf_bs_new(data_box->data, data_box->dataSize, GF_BITSTREAM_READ);
	switch (okey->type) {
	case GF_QT_KEY_UTF8:
	case GF_QT_KEY_UTF8_SORT:
		okey->value.string = data_box->data;
		break;

	case GF_QT_KEY_SIGNED_VSIZE:
	{
		u32 val = gf_bs_read_int(bs, nb_bits);
		if (nb_bits==8) okey->value.sint = (s64) (s8) val;
		else if (nb_bits==16) okey->value.sint = (s64) (s16) val;
		else if (nb_bits==32) okey->value.sint = (s64) (s32) val;
		else if (nb_bits==64) okey->value.sint = (s64) val;
	}
		break;
	case GF_QT_KEY_UNSIGNED_VSIZE:
		okey->value.uint = (s32) gf_bs_read_int(bs, nb_bits);
		break;
	case GF_QT_KEY_FLOAT:
		okey->value.number = gf_bs_read_float(bs);
		break;
	case GF_QT_KEY_DOUBLE:
		okey->value.number = gf_bs_read_double(bs);
		break;
	case GF_QT_KEY_SIGNED_8:
		okey->value.sint = (s64) (s8) gf_bs_read_int(bs, 8);
		break;
	case GF_QT_KEY_SIGNED_16:
		okey->value.sint = (s64) (s16) gf_bs_read_int(bs, 16);
		break;
	case GF_QT_KEY_SIGNED_32:
		okey->value.sint = (s64) (s32) gf_bs_read_int(bs, 32);
		break;
	case GF_QT_KEY_SIGNED_64:
		okey->value.sint = (s64) gf_bs_read_long_int(bs, 64);
		break;
	case GF_QT_KEY_POINTF:
	case GF_QT_KEY_SIZEF:
		okey->value.pos_size.x = gf_bs_read_float(bs);
		okey->value.pos_size.y = gf_bs_read_float(bs);
		break;
	case GF_QT_KEY_RECTF:
		okey->value.rect.x = gf_bs_read_float(bs);
		okey->value.rect.y = gf_bs_read_float(bs);
		okey->value.rect.w = gf_bs_read_float(bs);
		okey->value.rect.h = gf_bs_read_float(bs);
		break;

	case GF_QT_KEY_UNSIGNED_8:
		okey->value.uint = gf_bs_read_int(bs, 8);
		break;
	case GF_QT_KEY_UNSIGNED_16:
		okey->value.uint = gf_bs_read_int(bs, 16);
		break;
	case GF_QT_KEY_UNSIGNED_32:
		okey->value.uint = gf_bs_read_int(bs, 32);
		break;
	case GF_QT_KEY_UNSIGNED_64:
		okey->value.uint = gf_bs_read_int(bs, 64);
		break;
	case GF_QT_KEY_MATRIXF:
		for (i=0; i<9; i++)
			okey->value.matrix[i] = gf_bs_read_float(bs);
		break;

	case GF_QT_KEY_OPAQUE:
	case GF_QT_KEY_UTF16_BE:
	case GF_QT_KEY_JIS:
	case GF_QT_KEY_UTF16_SORT:
	case GF_QT_KEY_JPEG:
	case GF_QT_KEY_PNG:
	case GF_QT_KEY_BMP:
	case GF_QT_KEY_METABOX:
		okey->value.data.data = data_box->data;
		okey->value.data.data_len = data_box->dataSize;
		break;
	case GF_QT_KEY_REMOVE:
		break;
	}
	GF_Err e = GF_OK;
	if (gf_bs_is_overflow(bs))
		e = GF_ISOM_INVALID_FILE;
	gf_bs_del(bs);
	return e;
}

GF_EXPORT
GF_Err gf_isom_wma_enum_tag(GF_ISOFile *mov, u32 idx, char **out_tag, const u8 **data, u32 *data_len, u32 *version, u32 *data_type)
{
	GF_XtraBox *xtra;
	GF_XtraTag *tag;

	*out_tag = NULL;
	*data = NULL;
	*data_len = 0;
	*version = 0;
	*data_type = 0;

	xtra = (GF_XtraBox *) gf_isom_get_meta_extensions(mov, 1);
	if (!xtra) return GF_URL_ERROR;

	tag = gf_list_get(xtra->tags, idx);
	if (!tag) return GF_NOT_FOUND;
	*out_tag = tag->name;
	*data_len = tag->prop_size;
	*data = tag->prop_value;
	*version = tag->flags;
	*data_type = tag->prop_type;
	return GF_OK;
}


GF_EXPORT
GF_Err gf_isom_get_track_switch_group_count(GF_ISOFile *movie, u32 trackNumber, u32 *alternateGroupID, u32 *nb_groups)
{
	GF_UserDataMap *map;
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Header) return GF_BAD_PARAM;
	if (alternateGroupID) *alternateGroupID = trak->Header->alternate_group;
	if (nb_groups) *nb_groups = 0;
	if (!trak->udta || !nb_groups) return GF_OK;

	map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_TSEL, NULL);
	if (!map) return GF_OK;
	*nb_groups = gf_list_count(map->boxes);
	return GF_OK;
}

GF_EXPORT
const u32 *gf_isom_get_track_switch_parameter(GF_ISOFile *movie, u32 trackNumber, u32 group_index, u32 *switchGroupID, u32 *criteriaListSize)
{
	GF_TrackBox *trak;
	GF_UserDataMap *map;
	GF_TrackSelectionBox *tsel;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!group_index || !trak || !trak->udta) return NULL;

	map = udta_getEntry(trak->udta, GF_ISOM_BOX_TYPE_TSEL, NULL);
	if (!map) return NULL;
	tsel = (GF_TrackSelectionBox*)gf_list_get(map->boxes, group_index-1);
	if (!tsel) return NULL;

	*switchGroupID = tsel->switchGroup;
	*criteriaListSize = tsel->attributeListCount;
	return (const u32 *) tsel->attributeList;
}

GF_EXPORT
u32 gf_isom_get_next_alternate_group_id(GF_ISOFile *movie)
{
	u32 id = 0;
	u32 i=0;

	while (i< gf_isom_get_track_count(movie) ) {
		GF_TrackBox *trak = gf_isom_get_track_from_file(movie, i+1);
		if (trak->Header->alternate_group > id)
			id = trak->Header->alternate_group;
		i++;
	}
	return id+1;
}

GF_EXPORT
u8 *gf_isom_sample_get_subsamples_buffer(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 *osize)
{
	u8 *data;
	u32 size;
	u32 i, count;
	GF_BitStream *bs = NULL;
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, track);
	if (!trak || !osize) return NULL;
	if (!trak->Media || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->sub_samples) return NULL;

	count = gf_list_count(trak->Media->information->sampleTable->sub_samples);
	for (i=0; i<count; i++) {
		u32 j, sub_count, last_sample = 0;
		GF_SubSampleInformationBox *sub_samples = gf_list_get(trak->Media->information->sampleTable->sub_samples, i);

		sub_count = gf_list_count(sub_samples->Samples);
		for (j=0; j<sub_count; j++) {
			GF_SubSampleInfoEntry *pSamp = (GF_SubSampleInfoEntry *) gf_list_get(sub_samples->Samples, j);
			if (last_sample + pSamp->sample_delta == sampleNumber) {
				u32 scount = gf_list_count(pSamp->SubSamples);
				for (j=0; j<scount; j++) {
					GF_SubSampleEntry *sent = gf_list_get(pSamp->SubSamples, j);
					if (!bs) bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);

					gf_bs_write_u32(bs, sub_samples->flags);
					gf_bs_write_u32(bs, sent->subsample_size);
					gf_bs_write_u32(bs, sent->reserved);
					gf_bs_write_u8(bs, sent->subsample_priority);
					gf_bs_write_u8(bs, sent->discardable);
				}
				break;
			}
			last_sample += pSamp->sample_delta;
		}
	}
	if (!bs) return NULL;
	gf_bs_get_content(bs, &data, &size);
	gf_bs_del(bs);
	*osize = size;
	return data;
}

GF_EXPORT
u32 gf_isom_sample_has_subsamples(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 flags)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, track);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->sub_samples) return 0;
	if (!sampleNumber) return 1;
	return gf_isom_sample_get_subsample_entry(movie, track, sampleNumber, flags, NULL);
}

GF_EXPORT
GF_Err gf_isom_sample_get_subsample(GF_ISOFile *movie, u32 track, u32 sampleNumber, u32 flags, u32 subSampleNumber, u32 *size, u8 *priority, u32 *reserved, Bool *discardable)
{
	GF_SubSampleEntry *entry;
	GF_SubSampleInfoEntry *sub_sample;
	u32 count = gf_isom_sample_get_subsample_entry(movie, track, sampleNumber, flags, &sub_sample);
	if (!size || !priority || !discardable) return GF_BAD_PARAM;

	if (!subSampleNumber || (subSampleNumber>count)) return GF_BAD_PARAM;
	entry = (GF_SubSampleEntry*)gf_list_get(sub_sample->SubSamples, subSampleNumber-1);
	*size = entry->subsample_size;
	*priority = entry->subsample_priority;
	*reserved = entry->reserved;
	*discardable = entry->discardable ? GF_TRUE : GF_FALSE;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_rvc_config(GF_ISOFile *movie, u32 track, u32 sampleDescriptionIndex, u16 *rvc_predefined, u8 **data, u32 *size, const char **mime)
{
	GF_MPEGVisualSampleEntryBox *entry;
	GF_TrackBox *trak;

	if (!rvc_predefined || !data || !size) return GF_BAD_PARAM;
	*rvc_predefined = 0;

	trak = gf_isom_get_track_from_file(movie, track);
	if (!trak) return GF_BAD_PARAM;


	entry = (GF_MPEGVisualSampleEntryBox *) gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDescriptionIndex-1);
	if (!entry ) return GF_BAD_PARAM;
	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_BAD_PARAM;

	GF_RVCConfigurationBox *rvcc = (GF_RVCConfigurationBox *)gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_RVCC);
	if (!rvcc) return GF_NOT_FOUND;

	*rvc_predefined = rvcc->predefined_rvc_config;
	if (rvcc->rvc_meta_idx) {
		if (!data || !size) return GF_OK;
		return gf_isom_extract_meta_item_mem(movie, GF_FALSE, track, rvcc->rvc_meta_idx, data, size, NULL, mime, GF_FALSE);
	}
	return GF_OK;
}

GF_EXPORT
Bool gf_isom_moov_first(GF_ISOFile *movie)
{
	u32 i;
	for (i=0; i<gf_list_count(movie->TopBoxes); i++) {
		GF_Box *b = (GF_Box*)gf_list_get(movie->TopBoxes, i);
		if (b->type == GF_ISOM_BOX_TYPE_MOOV) return GF_TRUE;
		if (b->type == GF_ISOM_BOX_TYPE_MDAT) return GF_FALSE;
	}
	return GF_FALSE;
}

GF_EXPORT
void gf_isom_reset_fragment_info(GF_ISOFile *movie, Bool keep_sample_count)
{
	u32 i;
	if (!movie) return;
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(movie->moov->trackList, i);
		trak->Media->information->sampleTable->SampleSize->sampleCount = 0;
#ifdef GPAC_DISABLE_ISOM_FRAGMENTS
	}
#else
		//do not reset tfdt for LL-HLS case where parts do not contain a TFDT
		//trak->dts_at_seg_start = 0;
		if (!keep_sample_count)
			trak->sample_count_at_seg_start = 0;
	}
	movie->NextMoofNumber = 0;
#endif
}

GF_EXPORT
void gf_isom_reset_seq_num(GF_ISOFile *movie)
{
#ifdef GPAC_DISABLE_ISOM_FRAGMENTS
	movie->NextMoofNumber = 0;
#endif
}

GF_EXPORT
void gf_isom_reset_sample_count(GF_ISOFile *movie)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	u32 i;
	if (!movie) return;
	for (i=0; i<gf_list_count(movie->moov->trackList); i++) {
		GF_TrackBox *trak = (GF_TrackBox*)gf_list_get(movie->moov->trackList, i);
		trak->Media->information->sampleTable->SampleSize->sampleCount = 0;
		trak->sample_count_at_seg_start = 0;
	}
	movie->NextMoofNumber = 0;
#endif
}

GF_EXPORT
Bool gf_isom_has_cenc_sample_group(GF_ISOFile *the_file, u32 trackNumber, Bool *has_selective, Bool *has_roll)
{
	GF_TrackBox *trak;
	u32 i, count;
	GF_SampleGroupDescriptionBox *seig=NULL;

	if (has_selective) *has_selective = GF_FALSE;
	if (has_roll) *has_roll = GF_FALSE;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_FALSE;
	if (!trak->Media->information->sampleTable->sampleGroups) return GF_FALSE;

	count = gf_list_count(trak->Media->information->sampleTable->sampleGroupsDescription);
	for (i=0; i<count; i++) {
		GF_SampleGroupDescriptionBox *sgdesc = (GF_SampleGroupDescriptionBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroupsDescription, i);
		if (sgdesc->grouping_type==GF_ISOM_SAMPLE_GROUP_SEIG) {
			seig = sgdesc;
			break;
		}
	}
	if (!seig)
		return GF_FALSE;

	for (i=0; i<gf_list_count(seig->group_descriptions); i++) {
		GF_CENCSampleEncryptionGroupEntry *se = gf_list_get(seig->group_descriptions, i);
		if (!se->IsProtected) {
			if (has_selective) *has_selective = GF_TRUE;
		} else {
			if (has_roll) *has_roll = GF_TRUE;
		}
	}
	return GF_TRUE;
}

GF_EXPORT
GF_Err gf_isom_get_sample_rap_roll_info(GF_ISOFile *the_file, u32 trackNumber, u32 sample_number, Bool *is_rap, GF_ISOSampleRollType *roll_type, s32 *roll_distance)
{
	GF_TrackBox *trak;
	u32 i, count;

	if (is_rap) *is_rap = GF_FALSE;
	if (roll_type) *roll_type = 0;
	if (roll_distance) *roll_distance = 0;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->sampleGroups) return GF_OK;

	if (!sample_number) {
		count = gf_list_count(trak->Media->information->sampleTable->sampleGroupsDescription);
		for (i=0; i<count; i++) {
			GF_SampleGroupDescriptionBox *sgdesc = (GF_SampleGroupDescriptionBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroupsDescription, i);
			switch (sgdesc->grouping_type) {
			case GF_ISOM_SAMPLE_GROUP_RAP:
			case GF_ISOM_SAMPLE_GROUP_SYNC:
				if (is_rap) *is_rap = GF_TRUE;
				break;
			case GF_ISOM_SAMPLE_GROUP_ROLL:
			case GF_ISOM_SAMPLE_GROUP_PROL:
				if (roll_type)
					*roll_type = (sgdesc->grouping_type==GF_ISOM_SAMPLE_GROUP_PROL) ? GF_ISOM_SAMPLE_PREROLL : GF_ISOM_SAMPLE_ROLL;
				if (roll_distance) {
					s32 max_roll = 0;
					u32 j;
					for (j=0; j<gf_list_count(sgdesc->group_descriptions); j++) {
						GF_RollRecoveryEntry *roll_entry = (GF_RollRecoveryEntry*)gf_list_get(sgdesc->group_descriptions, j);
						if (max_roll < roll_entry->roll_distance)
							max_roll = roll_entry->roll_distance;
					}
					if (*roll_distance < max_roll) *roll_distance = max_roll;
				}
				break;
			}
		}
		return GF_OK;
	}

	count = gf_list_count(trak->Media->information->sampleTable->sampleGroups);
	for (i=0; i<count; i++) {
		GF_SampleGroupBox *sg;
		u32 j, group_desc_index;
		GF_SampleGroupDescriptionBox *sgdesc;
		u32 first_sample_in_entry, last_sample_in_entry;
		first_sample_in_entry = 1;
		group_desc_index = 0;
		sg = (GF_SampleGroupBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroups, i);
		for (j=0; j<sg->entry_count; j++) {
			last_sample_in_entry = first_sample_in_entry + sg->sample_entries[j].sample_count - 1;
			if ((sample_number<first_sample_in_entry) || (sample_number>last_sample_in_entry)) {
				first_sample_in_entry = last_sample_in_entry+1;
				continue;
			}
			/*we found our sample*/
			group_desc_index = sg->sample_entries[j].group_description_index;
			break;
		}
		/*no sampleGroup info associated*/
		if (!group_desc_index) continue;

		sgdesc = NULL;
		for (j=0; j<gf_list_count(trak->Media->information->sampleTable->sampleGroupsDescription); j++) {
			sgdesc = (GF_SampleGroupDescriptionBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroupsDescription, j);
			if (sgdesc->grouping_type==sg->grouping_type) break;
			sgdesc = NULL;
		}
		/*no sampleGroup description found for this group (invalid file)*/
		if (!sgdesc) continue;

		switch (sgdesc->grouping_type) {
		case GF_ISOM_SAMPLE_GROUP_RAP:
		case GF_ISOM_SAMPLE_GROUP_SYNC:
			if (is_rap) *is_rap = GF_TRUE;
			break;
		case GF_ISOM_SAMPLE_GROUP_ROLL:
		case GF_ISOM_SAMPLE_GROUP_PROL:
			if (roll_type)
				*roll_type = (sgdesc->grouping_type==GF_ISOM_SAMPLE_GROUP_PROL) ? GF_ISOM_SAMPLE_PREROLL : GF_ISOM_SAMPLE_ROLL;

			if (roll_distance) {
				GF_RollRecoveryEntry *roll_entry = (GF_RollRecoveryEntry *) gf_list_get(sgdesc->group_descriptions, group_desc_index - 1);
				if (roll_entry)
					*roll_distance = roll_entry->roll_distance;
			}
			break;
		}
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_sample_to_group_info(GF_ISOFile *the_file, u32 trackNumber, u32 sample_number, u32 grouping_type, u32 grouping_type_param, u32 *sampleGroupDescIndex)
{
	GF_TrackBox *trak;
	u32 i, count;

	if (!grouping_type || !sampleGroupDescIndex) return GF_BAD_PARAM;

	*sampleGroupDescIndex = 0;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->sampleGroups) return GF_OK;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sample_number <= trak->sample_count_at_seg_start) return GF_BAD_PARAM;
	sample_number -= trak->sample_count_at_seg_start;
#endif

	count = gf_list_count(trak->Media->information->sampleTable->sampleGroups);
	for (i=0; i<count; i++) {
		GF_SampleGroupBox *sg;
		u32 j;
		u32 first_sample_in_entry, last_sample_in_entry;
		first_sample_in_entry = 1;
		sg = (GF_SampleGroupBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroups, i);
		if (sg->grouping_type != grouping_type) continue;
		if (sg->grouping_type_parameter != grouping_type_param) continue;

		for (j=0; j<sg->entry_count; j++) {
			last_sample_in_entry = first_sample_in_entry + sg->sample_entries[j].sample_count - 1;
			if ((sample_number<first_sample_in_entry) || (sample_number>last_sample_in_entry)) {
				first_sample_in_entry = last_sample_in_entry+1;
				continue;
			}
			/*we found our sample*/
			*sampleGroupDescIndex = sg->sample_entries[j].group_description_index;
			return GF_OK;
		}
	}
	return GF_OK;
}

GF_Err gf_isom_enum_sample_group(GF_ISOFile *the_file, u32 trackNumber, u32 sample_number, u32 *sgrp_idx, u32 *sgrp_type, u32 *sgrp_parameter, const u8 **sgrp_data, u32 *sgrp_size)
{
	GF_TrackBox *trak;
	u32 i, count;

	if (!sgrp_idx || !sgrp_type) return GF_BAD_PARAM;
	if (sgrp_parameter) *sgrp_parameter = 0;
	if (sgrp_data) *sgrp_data = NULL;
	if (sgrp_size) *sgrp_size = 0;
	*sgrp_type = 0;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->sampleGroups) return GF_OK;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sample_number <= trak->sample_count_at_seg_start) return GF_BAD_PARAM;
	sample_number -= trak->sample_count_at_seg_start;
#endif

	count = gf_list_count(trak->Media->information->sampleTable->sampleGroupsDescription);
	for (i=0; i<count; i++) {
		GF_SampleGroupBox *sg;
		u32 j;
		GF_SampleGroupDescriptionBox *sgd = (GF_SampleGroupDescriptionBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroupsDescription, i);

		switch (sgd->grouping_type) {
		case GF_ISOM_SAMPLE_GROUP_ROLL:
		case GF_ISOM_SAMPLE_GROUP_PROL:
		case GF_ISOM_SAMPLE_GROUP_TELE:
		case GF_ISOM_SAMPLE_GROUP_RAP:
		case GF_ISOM_SAMPLE_GROUP_SYNC:
		case GF_ISOM_SAMPLE_GROUP_SEIG:
		case GF_ISOM_SAMPLE_GROUP_OINF:
		case GF_ISOM_SAMPLE_GROUP_LINF:
		case GF_ISOM_SAMPLE_GROUP_TRIF:
		case GF_ISOM_SAMPLE_GROUP_NALM:
		case GF_ISOM_SAMPLE_GROUP_SAP:
		case GF_ISOM_SAMPLE_GROUP_SPOR:
		case GF_ISOM_SAMPLE_GROUP_SULM:
			continue;
		default:
			break;
		}
		if (*sgrp_idx>i) continue;

		for (j=0; j<gf_list_count(trak->Media->information->sampleTable->sampleGroups); j++) {
			sg = (GF_SampleGroupBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroups, j);
			if (sg->grouping_type == sgd->grouping_type) break;
			sg = NULL;
		}
		u32 sgd_index = sgd->default_description_index;
		if (sg) {
			u32 snum=0;
			for (j=0; j<sg->entry_count; j++) {
				if (snum + sg->sample_entries[j].sample_count>= sample_number) {
					sgd_index = sg->sample_entries[j].group_description_index;
					break;
				}
				snum += sg->sample_entries[j].sample_count;
			}
		}

		*sgrp_type = sgd->grouping_type;
		if (sgrp_parameter && sg) *sgrp_parameter = sg->grouping_type_parameter;

		if (sgd_index) {
			GF_DefaultSampleGroupDescriptionEntry *entry = gf_list_get(sgd->group_descriptions, sgd_index-1);
			if (entry) {
				if (sgrp_data) *sgrp_data = entry->data;
				if (sgrp_size) *sgrp_size = entry->length;
			}
		}

		(*sgrp_idx)++;
		return GF_OK;
	}
	return GF_OK;
}


GF_DefaultSampleGroupDescriptionEntry * gf_isom_get_sample_group_info_entry(GF_ISOFile *the_file, GF_TrackBox *trak, u32 grouping_type, u32 sample_group_description_index, u32 *default_index, GF_SampleGroupDescriptionBox **out_sgdp)
{
	u32 i, count;

	if (!trak || !sample_group_description_index) return NULL;
	if (!trak->Media->information->sampleTable->sampleGroupsDescription) return NULL;

	count = gf_list_count(trak->Media->information->sampleTable->sampleGroupsDescription);
	for (i=0; i<count; i++) {
		GF_SampleGroupDescriptionBox *sgdesc = (GF_SampleGroupDescriptionBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroupsDescription, i);
		if (sgdesc->grouping_type != grouping_type) continue;

		if (sgdesc->default_description_index && !sample_group_description_index) sample_group_description_index = sgdesc->default_description_index;

		if (default_index) *default_index = sgdesc->default_description_index ;
		if (out_sgdp) *out_sgdp = sgdesc;

		if (!sample_group_description_index) return NULL;
		return (GF_DefaultSampleGroupDescriptionEntry*)gf_list_get(sgdesc->group_descriptions, sample_group_description_index-1);
	}
	return NULL;
}

GF_EXPORT
Bool gf_isom_get_sample_group_info(GF_ISOFile *the_file, u32 trackNumber, u32 sample_description_index, u32 grouping_type, u32 *default_index, const u8 **data, u32 *size)
{
	GF_DefaultSampleGroupDescriptionEntry *sg_entry;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);

	if (default_index) *default_index = 0;
	if (size) *size = 0;
	if (data) *data = NULL;

	sg_entry = gf_isom_get_sample_group_info_entry(the_file, trak, grouping_type, sample_description_index, default_index, NULL);
	if (!sg_entry) return GF_FALSE;

	switch (grouping_type) {
	case GF_ISOM_SAMPLE_GROUP_RAP:
	case GF_ISOM_SAMPLE_GROUP_SYNC:
	case GF_ISOM_SAMPLE_GROUP_ROLL:
	case GF_ISOM_SAMPLE_GROUP_SEIG:
	case GF_ISOM_SAMPLE_GROUP_OINF:
	case GF_ISOM_SAMPLE_GROUP_LINF:
		return GF_TRUE;
	default:
		if (sg_entry && data) *data = (char *) sg_entry->data;
		if (sg_entry && size) *size = sg_entry->length;
		return GF_TRUE;
	}
	return GF_FALSE;
}

#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
//return the duration of the movie+fragments if known, 0 if error
GF_EXPORT
u64 gf_isom_get_fragmented_duration(GF_ISOFile *movie)
{
	if (movie->moov->mvex && movie->moov->mvex->mehd)
		return movie->moov->mvex->mehd->fragment_duration;

	return 0;
}
//return the duration of the movie+fragments if known, 0 if error
GF_EXPORT
u32 gf_isom_get_fragments_count(GF_ISOFile *movie, Bool segments_only)
{
	u32 i=0;
	u32 nb_frags = 0;
	GF_Box *b;
	while ((b=(GF_Box*)gf_list_enum(movie->TopBoxes, &i))) {
		if (segments_only) {
			if (b->type==GF_ISOM_BOX_TYPE_SIDX)
				nb_frags++;
		} else {
			if (b->type==GF_ISOM_BOX_TYPE_MOOF)
				nb_frags++;
		}
	}
	return nb_frags;
}

GF_EXPORT
GF_Err gf_isom_get_fragmented_samples_info(GF_ISOFile *movie, GF_ISOTrackID trackID, u32 *nb_samples, u64 *duration)
{
	u32 i=0;
	u32 k, l;
	GF_MovieFragmentBox *moof;
	GF_TrackFragmentBox *traf;

	*nb_samples = 0;
	*duration = 0;
	while ((moof=(GF_MovieFragmentBox*)gf_list_enum(movie->TopBoxes, &i))) {
		u32 j=0;
		if (moof->type!=GF_ISOM_BOX_TYPE_MOOF) continue;

		while ((traf=(GF_TrackFragmentBox*)gf_list_enum( moof->TrackList, &j))) {
			u64 def_duration, samp_dur=0;

			if (traf->tfhd->trackID != trackID)
				continue;

			def_duration = 0;
			if (traf->tfhd->flags & GF_ISOM_TRAF_SAMPLE_DUR) def_duration = traf->tfhd->def_sample_duration;
			else if (traf->trex) def_duration = traf->trex->def_sample_duration;

			for (k=0; k<gf_list_count(traf->TrackRuns); k++) {
				GF_TrackFragmentRunBox *trun = (GF_TrackFragmentRunBox*)gf_list_get(traf->TrackRuns, k);
				*nb_samples += trun->sample_count;

				for (l=0; l<trun->nb_samples; l++) {
					GF_TrunEntry *ent = &trun->samples[l];

					samp_dur = def_duration;
					if (trun->flags & GF_ISOM_TRUN_DURATION) samp_dur = ent->Duration;
					if (trun->nb_samples == trun->sample_count)
						*duration += samp_dur;
				}
				if (trun->nb_samples != trun->sample_count)
					*duration += samp_dur * trun->sample_count;
			}
		}
	}
	return GF_OK;
}
#endif

GF_EXPORT
GF_Err gf_isom_set_nalu_extract_mode(GF_ISOFile *the_file, u32 trackNumber, GF_ISONaluExtractMode nalu_extract_mode)
{
	GF_TrackReferenceTypeBox *dpnd;
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	trak->extractor_mode = nalu_extract_mode;

	if (!trak->References) return GF_OK;

	/*get base*/
	dpnd = NULL;
	trak->has_base_layer = GF_FALSE;
	Track_FindRef(trak, GF_ISOM_REF_SCAL, &dpnd);
	if (dpnd) trak->has_base_layer = GF_TRUE;
	return GF_OK;
}

GF_EXPORT
GF_ISONaluExtractMode gf_isom_get_nalu_extract_mode(GF_ISOFile *the_file, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;
	return trak->extractor_mode;
}

GF_EXPORT
s32 gf_isom_get_composition_offset_shift(GF_ISOFile *file, u32 track)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(file, track);
	if (!trak) return 0;
	if (!trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->CompositionToDecode) return 0;
	return trak->Media->information->sampleTable->CompositionToDecode->compositionToDTSShift;
}

GF_EXPORT
Bool gf_isom_needs_layer_reconstruction(GF_ISOFile *file)
{
	u32 count, i;
	if (!file)
		return GF_FALSE;
	count = gf_isom_get_track_count(file);
	for (i = 0; i < count; i++) {
		if (gf_isom_get_reference_count(file, i+1, GF_ISOM_REF_SCAL) > 0) {
			return GF_TRUE;
		}
		if (gf_isom_get_reference_count(file, i+1, GF_ISOM_REF_SABT) > 0) {
			return GF_TRUE;
		}
		switch (gf_isom_get_media_subtype(file, i+1, 1)) {
		case GF_ISOM_SUBTYPE_LHV1:
		case GF_ISOM_SUBTYPE_LHE1:
		case GF_ISOM_SUBTYPE_HVC2:
		case GF_ISOM_SUBTYPE_HEV2:
			if (gf_isom_get_reference_count(file, i+1, GF_ISOM_REF_BASE) > 0) {
				return GF_TRUE;
			}
		}
	}
	return GF_FALSE;
}

GF_EXPORT
void gf_isom_keep_utc_times(GF_ISOFile *file, Bool keep_utc)
{
	if (!file) return;
	file->keep_utc = keep_utc;
}

GF_EXPORT
Bool gf_isom_has_keep_utc_times(GF_ISOFile *file)
{
	if (!file) return GF_FALSE;
	return file->keep_utc;
}



GF_EXPORT
u32 gf_isom_get_pssh_count(GF_ISOFile *file)
{
	u32 count=0;
	u32 i=0;
	GF_Box *a_box;
	if (file->moov) {
		while ((a_box = (GF_Box*)gf_list_enum(file->moov->child_boxes, &i))) {
			if (a_box->type != GF_ISOM_BOX_TYPE_PSSH) continue;
			count++;
		}
	}
	if (file->meta) {
		while ((a_box = (GF_Box*)gf_list_enum(file->meta->child_boxes, &i))) {
			if (a_box->type != GF_ISOM_BOX_TYPE_PSSH) continue;
			count++;
		}
	}
	return count;
}

#if 0 //unused
/*! gets serialized PSS
\param isom_file the target ISO file
\param pssh_index 1-based index of PSSH to query, see \ref gf_isom_get_pssh_count
\param pssh_data set to a newly allocated buffer containing serialized PSSH - shall be freeed by caller
\param pssh_size set to the size of the allocated buffer
\return error if any
*/
GF_Err gf_isom_get_pssh(GF_ISOFile *file, u32 pssh_index, u8 **pssh_data, u32 *pssh_size)
{
	GF_Err e;
	u32 i=0;
	GF_BitStream *bs;
	u32 count=1;
	GF_Box *pssh;
	while ((pssh = (GF_Box *)gf_list_enum(file->moov->child_boxes, &i))) {
		if (pssh->type != GF_ISOM_BOX_TYPE_PSSH) continue;
		if (count == pssh_index) break;
		count++;
	}
	if (!pssh) return GF_BAD_PARAM;
	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	e = gf_isom_box_write(pssh, bs);
	if (!e) {
		gf_bs_get_content(bs, pssh_data, pssh_size);
	}
	gf_bs_del(bs);
	return e;
}
#endif

GF_EXPORT
GF_Err gf_isom_get_pssh_info(GF_ISOFile *file, u32 pssh_index, bin128 SystemID, u32 *version, u32 *KID_count, const bin128 **KIDs, const u8 **private_data, u32 *private_data_size)
{
	u32 count=1;
	u32 i=0;
	GF_ProtectionSystemHeaderBox *pssh=NULL;
	if (file->moov) {
		while ((pssh = (GF_ProtectionSystemHeaderBox *)gf_list_enum(file->moov->child_boxes, &i))) {
			if (pssh->type != GF_ISOM_BOX_TYPE_PSSH) continue;
			if (count == pssh_index) break;
			count++;
		}
	}
	if (!pssh && file->meta) {
		while ((pssh = (GF_ProtectionSystemHeaderBox *)gf_list_enum(file->meta->child_boxes, &i))) {
			if (pssh->type != GF_ISOM_BOX_TYPE_PSSH) continue;
			if (count == pssh_index) break;
			count++;
		}
	}
	if (!pssh) return GF_BAD_PARAM;

	if (SystemID) memcpy(SystemID, pssh->SystemID, 16);
	if (version) *version = pssh->version;
	if (KID_count) *KID_count = pssh->KID_count;
	if (KIDs) *KIDs = (const bin128 *) pssh->KIDs;
	if (private_data_size) *private_data_size = pssh->private_data_size;
	if (private_data) *private_data = pssh->private_data;
	return GF_OK;
}

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
GF_Err gf_isom_get_sample_cenc_info_internal(GF_TrackBox *trak, GF_TrackFragmentBox *traf, GF_SampleEncryptionBox *senc, u32 sample_number, Bool *IsEncrypted, u8 *crypt_byte_block, u8 *skip_byte_block, const u8 **key_info, u32 *key_info_size)
#else
GF_Err gf_isom_get_sample_cenc_info_internal(GF_TrackBox *trak, void *traf, GF_SampleEncryptionBox *senc, u32 sample_number, Bool *IsEncrypted, u8 *crypt_byte_block, u8 *skip_byte_block, const u8 **key_info, u32 *key_info_size)
#endif
{
	GF_SampleGroupBox *sample_group;
	u32 j, group_desc_index;
	GF_SampleGroupDescriptionBox *sgdesc;
	u32 i, count;
	u32 descIndex, chunkNum;
	u64 offset;
	u32 first_sample_in_entry, last_sample_in_entry;
	GF_CENCSampleEncryptionGroupEntry *entry;

	if (IsEncrypted) *IsEncrypted = GF_FALSE;
	if (crypt_byte_block) *crypt_byte_block = 0;
	if (skip_byte_block) *skip_byte_block = 0;
	if (key_info) *key_info = NULL;
	if (key_info_size) *key_info_size = 0;

	if (!trak) return GF_BAD_PARAM;

#ifdef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (traf)
		return GF_NOT_SUPPORTED;
#else
	sample_number -= trak->sample_count_at_seg_start;
#endif

	if (trak->Media->information->sampleTable->SampleSize && trak->Media->information->sampleTable->SampleSize->sampleCount>=sample_number) {
		stbl_GetSampleInfos(trak->Media->information->sampleTable, sample_number, &offset, &chunkNum, &descIndex, NULL);
	} else {
		//this is dump mode of fragments, we haven't merged tables yet - use current stsd idx indicated in trak
		descIndex = trak->current_traf_stsd_idx;
		if (!descIndex) descIndex = 1;
	}

	gf_isom_cenc_get_default_info_internal(trak, descIndex, NULL, IsEncrypted, crypt_byte_block, skip_byte_block, key_info, key_info_size);

	sample_group = NULL;
	group_desc_index = 0;
	if (trak->Media->information->sampleTable->sampleGroups) {
		count = gf_list_count(trak->Media->information->sampleTable->sampleGroups);
		for (i=0; i<count; i++) {
			sample_group = (GF_SampleGroupBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroups, i);
			if (sample_group->grouping_type ==  GF_ISOM_SAMPLE_GROUP_SEIG)
				break;
			sample_group = NULL;
		}
		if (sample_group) {
			first_sample_in_entry = 1;
			for (j=0; j<sample_group->entry_count; j++) {
				last_sample_in_entry = first_sample_in_entry + sample_group->sample_entries[j].sample_count - 1;
				if ((sample_number<first_sample_in_entry) || (sample_number>last_sample_in_entry)) {
					first_sample_in_entry = last_sample_in_entry+1;
					continue;
				}
				/*we found our sample*/
				group_desc_index = sample_group->sample_entries[j].group_description_index;
				break;
			}
		}
	}
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	if (!group_desc_index && traf && traf->sampleGroups) {
		count = gf_list_count(traf->sampleGroups);
		for (i=0; i<count; i++) {
			group_desc_index = 0;
			sample_group = (GF_SampleGroupBox*)gf_list_get(traf->sampleGroups, i);
			if (sample_group->grouping_type ==  GF_ISOM_SAMPLE_GROUP_SEIG)
				break;
			sample_group = NULL;
		}
		if (sample_group) {
			first_sample_in_entry = 1;
			for (j=0; j<sample_group->entry_count; j++) {
				last_sample_in_entry = first_sample_in_entry + sample_group->sample_entries[j].sample_count - 1;
				if ((sample_number<first_sample_in_entry) || (sample_number>last_sample_in_entry)) {
					first_sample_in_entry = last_sample_in_entry+1;
					continue;
				}
				/*we found our sample*/
				group_desc_index = sample_group->sample_entries[j].group_description_index;
				break;
			}
		}
	}
#endif
	/*no sampleGroup info associated*/
	if (!group_desc_index) goto exit;

	sgdesc = NULL;

	if (group_desc_index<=0x10000) {
		for (j=0; j<gf_list_count(trak->Media->information->sampleTable->sampleGroupsDescription); j++) {
			sgdesc = (GF_SampleGroupDescriptionBox*)gf_list_get(trak->Media->information->sampleTable->sampleGroupsDescription, j);
			if (sgdesc->grouping_type==sample_group->grouping_type) break;
			sgdesc = NULL;
		}
	}
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	else if (traf) {
		group_desc_index -= 0x10000;
		for (j=0; j<gf_list_count(traf->sampleGroupsDescription); j++) {
			sgdesc = (GF_SampleGroupDescriptionBox*)gf_list_get(traf->sampleGroupsDescription, j);
			if (sgdesc->grouping_type==sample_group->grouping_type) break;
			sgdesc = NULL;
		}
	}
#endif
	/*no sampleGroup description found for this group (invalid file)*/
	if (!sgdesc) return GF_ISOM_INVALID_FILE;

	entry = (GF_CENCSampleEncryptionGroupEntry *) gf_list_get(sgdesc->group_descriptions, group_desc_index - 1);
	if (!entry) return GF_ISOM_INVALID_FILE;

	if (IsEncrypted) *IsEncrypted = entry->IsProtected;
	if (crypt_byte_block) *crypt_byte_block = entry->crypt_byte_block;
	if (skip_byte_block) *skip_byte_block = entry->skip_byte_block;

	if (key_info) *key_info = entry->key_info;
	if (key_info_size) *key_info_size = entry->key_info_size;

exit:
	//in PIFF we may have default values if no TENC is present: 8 bytes for IV size
	if (( (senc && senc->piff_type==1) || (trak->moov && trak->moov->mov->is_smooth) ) && key_info && ! (*key_info) ) {
		if (!senc) {
			if (IsEncrypted) *IsEncrypted = GF_TRUE;
			if (key_info_size) *key_info_size = 8;
		} else {
			if (!senc->piff_type) {
				senc->piff_type = 2;
				senc->IV_size = 8;
			}
			assert(senc->IV_size);
			if (IsEncrypted) *IsEncrypted = GF_TRUE;
			if (key_info_size) *key_info_size = senc->IV_size;
		}
	}

	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_sample_cenc_info(GF_ISOFile *movie, u32 track, u32 sample_number, Bool *IsEncrypted, u8 *crypt_byte_block, u8 *skip_byte_block, const u8 **key_info, u32 *key_info_size)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, track);
	GF_SampleEncryptionBox *senc = trak->sample_encryption;

	return gf_isom_get_sample_cenc_info_internal(trak, NULL, senc, sample_number, IsEncrypted, crypt_byte_block, skip_byte_block, key_info, key_info_size);
}


GF_EXPORT
Bool gf_isom_get_last_producer_time_box(GF_ISOFile *file, GF_ISOTrackID *refTrackID, u64 *ntp, u64 *timestamp, Bool reset_info)
{
	if (!file) return GF_FALSE;
	if (refTrackID) *refTrackID = 0;
	if (ntp) *ntp = 0;
	if (timestamp) *timestamp = 0;

	if (file->last_producer_ref_time) {
		if (refTrackID) *refTrackID = file->last_producer_ref_time->refTrackID;
		if (ntp) *ntp = file->last_producer_ref_time->ntp;
		if (timestamp) *timestamp = file->last_producer_ref_time->timestamp;

		if (reset_info) {
			file->last_producer_ref_time = NULL;
		}
		return GF_TRUE;
	}
	return GF_FALSE;
}

GF_EXPORT
u64 gf_isom_get_current_tfdt(GF_ISOFile *the_file, u32 trackNumber)
{
#ifdef	GPAC_DISABLE_ISOM_FRAGMENTS
	return 0;
#else
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;
	return trak->dts_at_seg_start;
#endif
}

GF_EXPORT
u64 gf_isom_get_smooth_next_tfdt(GF_ISOFile *the_file, u32 trackNumber)
{
#ifdef	GPAC_DISABLE_ISOM_FRAGMENTS
	return 0;
#else
	GF_TrackBox *trak;
	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return 0;
	return trak->dts_at_next_frag_start;
#endif
}

GF_EXPORT
Bool gf_isom_is_smooth_streaming_moov(GF_ISOFile *the_file)
{
	return the_file ? the_file->is_smooth : GF_FALSE;
}


void gf_isom_parse_trif_info(const u8 *data, u32 size, u32 *id, u32 *independent, Bool *full_picture, u32 *x, u32 *y, u32 *w, u32 *h)
{
	GF_BitStream *bs;
	bs = gf_bs_new(data, size, GF_BITSTREAM_READ);
	*id = gf_bs_read_u16(bs);
	if (! gf_bs_read_int(bs, 1)) {
		*independent=0;
		*full_picture=0;
		*x = *y = *w = *h = 0;
	} else {
		*independent = gf_bs_read_int(bs, 2);
		*full_picture = (Bool)gf_bs_read_int(bs, 1);
		/*filter_disabled*/ gf_bs_read_int(bs, 1);
		/*has_dependency_list*/ gf_bs_read_int(bs, 1);
		gf_bs_read_int(bs, 2);
		*x = *full_picture ? 0 : gf_bs_read_u16(bs);
		*y = *full_picture ? 0 : gf_bs_read_u16(bs);
		*w = gf_bs_read_u16(bs);
		*h = gf_bs_read_u16(bs);
	}
	gf_bs_del(bs);
}

GF_EXPORT
Bool gf_isom_get_tile_info(GF_ISOFile *file, u32 trackNumber, u32 sample_description_index, u32 *default_sample_group_index, u32 *id, u32 *independent, Bool *full_picture, u32 *x, u32 *y, u32 *w, u32 *h)
{
	const u8 *data;
	u32 size;

	if (!gf_isom_get_sample_group_info(file, trackNumber, sample_description_index, GF_ISOM_SAMPLE_GROUP_TRIF, default_sample_group_index, &data, &size))
		return GF_FALSE;
	gf_isom_parse_trif_info(data, size, id, independent, full_picture, x, y, w, h);
	return GF_TRUE;
}

GF_EXPORT
Bool gf_isom_get_oinf_info(GF_ISOFile *file, u32 trackNumber, GF_OperatingPointsInformation **ptr)
{
	u32 oref_track, def_index=0;
	GF_TrackBox *trak = gf_isom_get_track_from_file(file, trackNumber);

	if (!ptr) return GF_FALSE;

	oref_track=0;
	gf_isom_get_reference(file, trackNumber, GF_ISOM_REF_OREF, 1, &oref_track);
	if (oref_track) {
		trak = gf_isom_get_track_from_file(file, oref_track);
		if (!trak) return GF_FALSE;
	}

	*ptr = (GF_OperatingPointsInformation *) gf_isom_get_sample_group_info_entry(file, trak, GF_ISOM_SAMPLE_GROUP_OINF, 1, &def_index, NULL);

	return *ptr ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
GF_Err gf_isom_set_byte_offset(GF_ISOFile *file, s64 byte_offset)
{
	if (!file) return GF_BAD_PARAM;
	file->read_byte_offset = byte_offset;
	return GF_OK;
}

GF_EXPORT
u32 gf_isom_get_nalu_length_field(GF_ISOFile *file, u32 track, u32 StreamDescriptionIndex)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_MPEGVisualSampleEntryBox *ve;
	GF_SampleDescriptionBox *stsd;

	trak = gf_isom_get_track_from_file(file, track);
	if (!trak) {
		file->LastError = GF_BAD_PARAM;
		return 0;
	}

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd || !StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		file->LastError = GF_BAD_PARAM;
		return 0;
	}

	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	//no support for generic sample entries (eg, no MPEG4 descriptor)
	if (!entry || ! gf_isom_is_nalu_based_entry(trak->Media, entry)) {
		file->LastError = GF_BAD_PARAM;
		return 0;
	}

	ve = (GF_MPEGVisualSampleEntryBox*)entry;
	if (ve->avc_config) return ve->avc_config->config->nal_unit_size;
	if (ve->svc_config) return ve->svc_config->config->nal_unit_size;
	if (ve->hevc_config) return ve->hevc_config->config->nal_unit_size;
	if (ve->lhvc_config) return ve->lhvc_config->config->nal_unit_size;
	return 0;
}

GF_EXPORT
Bool gf_isom_is_video_handler_type(u32 mtype)
{
	switch (mtype) {
	case GF_ISOM_MEDIA_VISUAL:
	case GF_ISOM_MEDIA_AUXV:
	case GF_ISOM_MEDIA_PICT:
		return GF_TRUE;
	default:
		return GF_FALSE;
	}
}

GF_EXPORT
GF_Err gf_isom_get_bitrate(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescIndex, u32 *average_bitrate, u32 *max_bitrate, u32 *decode_buffer_size)
{
	GF_BitRateBox *a;
	u32 i, count, mrate, arate, dbsize, type;
	GF_SampleEntryBox *ent;
	GF_ProtectionSchemeInfoBox *sinf;
	GF_TrackBox *trak;
	GF_ESDBox *esd;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_BAD_PARAM;

	mrate = arate = dbsize = 0;
	count = gf_list_count(trak->Media->information->sampleTable->SampleDescription->child_boxes);
	for (i=0; i<count; i++) {
		if ((sampleDescIndex>0) && (i+1 != sampleDescIndex)) continue;

		ent = (GF_SampleEntryBox *)gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, i);
		if (!ent) return GF_BAD_PARAM;
		a = gf_isom_sample_entry_get_bitrate(ent, GF_FALSE);
		if (a) {
			if (mrate<a->maxBitrate) mrate = a->maxBitrate;
			if (arate<a->avgBitrate) arate = a->avgBitrate;
			if (dbsize<a->bufferSizeDB) dbsize = a->bufferSizeDB;
			continue;
		}
		type = ent->type;
		switch (type) {
		case GF_ISOM_BOX_TYPE_ENCV:
		case GF_ISOM_BOX_TYPE_ENCA:
		case GF_ISOM_BOX_TYPE_ENCS:
			sinf = (GF_ProtectionSchemeInfoBox *) gf_isom_box_find_child(ent->child_boxes, GF_ISOM_BOX_TYPE_SINF);
			if (sinf && sinf->original_format) type = sinf->original_format->data_format;
			break;
		}
		esd = NULL;
		switch (type) {
		case GF_ISOM_BOX_TYPE_MP4V:
			esd = ((GF_MPEGVisualSampleEntryBox *)ent)->esd;
			break;
		case GF_ISOM_BOX_TYPE_MP4A:
			esd = ((GF_MPEGAudioSampleEntryBox *)ent)->esd;
			break;
		case GF_ISOM_BOX_TYPE_MP4S:
			esd = ((GF_MPEGSampleEntryBox *)ent)->esd;
			break;
		}
		if (esd && esd->desc && esd->desc->decoderConfig) {
			if (mrate<esd->desc->decoderConfig->maxBitrate) mrate = esd->desc->decoderConfig->maxBitrate;
			if (arate<esd->desc->decoderConfig->avgBitrate) arate = esd->desc->decoderConfig->avgBitrate;
			if (dbsize<esd->desc->decoderConfig->bufferSizeDB) dbsize = esd->desc->decoderConfig->bufferSizeDB;
		}
	}
	if (average_bitrate) *average_bitrate = arate;
	if (max_bitrate) *max_bitrate = mrate;
	if (decode_buffer_size) *decode_buffer_size = dbsize;
	return GF_OK;
}

void gf_isom_enable_traf_map_templates(GF_ISOFile *movie)
{
	if (movie)
		movie->signal_frag_bounds=GF_TRUE;
}

GF_EXPORT
Bool gf_isom_sample_is_fragment_start(GF_ISOFile *movie, u32 trackNumber, u32 sampleNum, GF_ISOFragmentBoundaryInfo *frag_info)
{
	u32 i;
	GF_TrackBox *trak;
	GF_TrafToSampleMap *tmap;

	if (frag_info) memset(frag_info, 0, sizeof(GF_ISOFragmentBoundaryInfo));

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media) return GF_FALSE;
	if (!trak->Media->information->sampleTable->traf_map) return GF_FALSE;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sampleNum<=trak->sample_count_at_seg_start)
		return GF_FALSE;
	sampleNum -= trak->sample_count_at_seg_start;
#endif

	tmap = trak->Media->information->sampleTable->traf_map;
	if (!tmap) return GF_FALSE;
	for (i=0; i<tmap->nb_entries; i++) {
		GF_TrafMapEntry *finfo = &tmap->frag_starts[i];
		if (finfo->sample_num == sampleNum) {
			if (frag_info) {
				frag_info->frag_start = finfo->moof_start;
				frag_info->mdat_end = finfo->mdat_end;
				frag_info->moof_template = finfo->moof_template;
				frag_info->moof_template_size = finfo->moof_template_size;
				frag_info->seg_start_plus_one = finfo->seg_start_plus_one;
				frag_info->sidx_start = finfo->sidx_start;
				frag_info->sidx_end = finfo->sidx_end;
			}
			return GF_TRUE;
		}

		if (tmap->frag_starts[i].sample_num > sampleNum) return GF_FALSE;
	}
	return GF_FALSE;
}

GF_EXPORT
Bool gf_isom_get_root_sidx_offsets(GF_ISOFile *movie, u64 *start, u64 *end)
{
	if (!movie || !start || !end) return GF_FALSE;
	*start = movie->root_sidx_start_offset;
	*end = movie->root_sidx_end_offset;
	return GF_TRUE;
}


GF_EXPORT
GF_Err gf_isom_get_jp2_config(GF_ISOFile *movie, u32 trackNumber, u32 sampleDesc, u8 **out_dsi, u32 *out_size)
{
	GF_TrackBox *trak;
	GF_MPEGVisualSampleEntryBox *entry;
	GF_BitStream *bs;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->SampleDescription) return GF_ISOM_INVALID_FILE;
	entry = (GF_MPEGVisualSampleEntryBox *) gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDesc-1);
	if (!entry || !entry->jp2h) return GF_BAD_PARAM;
	if (!entry->jp2h->ihdr) return GF_ISOM_INVALID_FILE;

	bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	gf_isom_box_array_write((GF_Box*)entry->jp2h, entry->jp2h->child_boxes, bs);
	gf_bs_get_content(bs, out_dsi, out_size);
	gf_bs_del(bs);
	return GF_OK;
}


Bool gf_isom_is_identical_sgpd(void *ptr1, void *ptr2, u32 grouping_type)
{
	Bool res = GF_FALSE;
#ifndef GPAC_DISABLE_ISOM_WRITE
	GF_BitStream *bs1, *bs2;
	u8 *buf1, *buf2;
	u32 len1, len2;

	if (!ptr1 || !ptr2)
		return GF_FALSE;

	bs1 = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (grouping_type) {
		sgpd_write_entry(grouping_type, ptr1, bs1);
	} else {
		gf_isom_box_size((GF_Box *)ptr1);
		gf_isom_box_write((GF_Box *)ptr1, bs1);
	}
	gf_bs_get_content(bs1, &buf1, &len1);
	gf_bs_del(bs1);

	bs2 = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	if (grouping_type) {
		sgpd_write_entry(grouping_type, ptr2, bs2);
	} else {
		gf_isom_box_write((GF_Box *)ptr2, bs2);
	}
	gf_bs_get_content(bs2, &buf2, &len2);
	gf_bs_del(bs2);


	if ((len1==len2) && !memcmp(buf1, buf2, len1))
		res = GF_TRUE;

	gf_free(buf1);
	gf_free(buf2);
#endif
	return res;
}

GF_EXPORT
u64 gf_isom_get_track_magic(GF_ISOFile *movie, u32 trackNumber)
{
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return 0;
	return trak->magic;
}

GF_EXPORT
GF_Err gf_isom_get_file_offset_for_time(GF_ISOFile *movie, Double start_time, u64 *max_offset)
{
	u32 i;
	u64 start_ts, cur_start_time;
	u64 offset=0;
	if (!movie || !movie->moov)
		return GF_BAD_PARAM;

	if (!movie->main_sidx) return GF_NOT_SUPPORTED;
	start_ts = (u64) (start_time * movie->main_sidx->timescale);
	cur_start_time = 0;
	offset = movie->main_sidx->first_offset + movie->main_sidx_end_pos;

	for (i=0; i<movie->main_sidx->nb_refs; i++) {
		if (cur_start_time >= start_ts) {
			*max_offset = offset;
			return GF_OK;
		}
		cur_start_time += movie->main_sidx->refs[i].subsegment_duration;
		offset += movie->main_sidx->refs[i].reference_size;
	}

	return GF_EOS;
}

GF_EXPORT
GF_Err gf_isom_get_sidx_duration(GF_ISOFile *movie, u64 *sidx_dur, u32 *sidx_timescale)
{
	u64 dur=0;
	u32 i;
	if (!movie || !movie->moov || !sidx_timescale || !sidx_dur)
		return GF_BAD_PARAM;

	if (!movie->main_sidx) return GF_NOT_SUPPORTED;
	*sidx_timescale = movie->main_sidx->timescale;

	for (i=0; i<movie->main_sidx->nb_refs; i++) {
		dur += movie->main_sidx->refs[i].subsegment_duration;
	}
	*sidx_dur = dur;
	return GF_OK;
}

GF_EXPORT
const u8 *gf_isom_get_mpegh_compatible_profiles(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescIndex, u32 *nb_compat_profiles)
{
	GF_SampleEntryBox *ent;
	GF_MHACompatibleProfilesBox *mhap;
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !nb_compat_profiles) return NULL;
	*nb_compat_profiles = 0;
	ent = gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDescIndex-1);
	if (!ent) return NULL;
	mhap = (GF_MHACompatibleProfilesBox *) gf_isom_box_find_child(ent->child_boxes, GF_ISOM_BOX_TYPE_MHAP);
	if (!mhap) return NULL;
	*nb_compat_profiles = mhap->num_profiles;
	return mhap->compat_profiles;
}

const void *gf_isom_get_tfrf(GF_ISOFile *movie, u32 trackNumber)
{
#ifdef GPAC_DISABLE_ISOM_FRAGMENTS
	return NULL;
#else
	GF_TrackBox *trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return NULL;

	return trak->tfrf;
#endif
}

GF_Err gf_isom_get_y3d_info(GF_ISOFile *movie, u32 trackNumber, u32 sampleDescriptionIndex, GF_ISOM_Y3D_Info *info)
{
	GF_SampleEntryBox *ent;
	GF_TrackBox *trak;
	Bool found = GF_FALSE;
	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !info) return GF_BAD_PARAM;

	ent = gf_list_get(trak->Media->information->sampleTable->SampleDescription->child_boxes, sampleDescriptionIndex-1);
	if (!ent) return GF_BAD_PARAM;

	memset(info, 0, sizeof(GF_ISOM_Y3D_Info));

	GF_Stereo3DBox *st3d = (GF_Stereo3DBox *) gf_isom_box_find_child(ent->child_boxes, GF_ISOM_BOX_TYPE_ST3D);
	if (st3d) {
		found = GF_TRUE;
		info->stereo_type = st3d->stereo_type;
	}

	GF_Box *sv3d = gf_isom_box_find_child(ent->child_boxes, GF_ISOM_BOX_TYPE_SV3D);
	if (!sv3d) {
		return found ? GF_OK : GF_NOT_FOUND;
	}
	GF_SphericalVideoInfoBox *svhd = (GF_SphericalVideoInfoBox *) gf_isom_box_find_child(sv3d->child_boxes, GF_ISOM_BOX_TYPE_SVHD);
	if (svhd && strlen(svhd->string)) info->meta_data = svhd->string;

	GF_Box *proj = gf_isom_box_find_child(sv3d->child_boxes, GF_ISOM_BOX_TYPE_PROJ);
	if (!proj)
		return found ? GF_OK : GF_NOT_FOUND;

	GF_ProjectionHeaderBox *projh = (GF_ProjectionHeaderBox *) gf_isom_box_find_child(proj->child_boxes, GF_ISOM_BOX_TYPE_PRHD);
	if (projh) {
		info->yaw = projh->yaw;
		info->pitch = projh->pitch;
		info->roll = projh->roll;
		info->pose_present = GF_TRUE;
	}

	GF_ProjectionTypeBox *projt = (GF_ProjectionTypeBox *) gf_isom_box_find_child(proj->child_boxes, GF_ISOM_BOX_TYPE_CBMP);
	if (projt) {
		info->layout = projt->layout;
		info->padding = projt->padding;
		info->projection_type = 1;
	} else {
		projt = (GF_ProjectionTypeBox *) gf_isom_box_find_child(proj->child_boxes, GF_ISOM_BOX_TYPE_EQUI);
		if (projt) {
			info->top = projt->bounds_top;
			info->bottom = projt->bounds_bottom;
			info->left = projt->bounds_left;
			info->right = projt->bounds_right;
			info->projection_type = 2;
		} else {
			projt = (GF_ProjectionTypeBox *) gf_isom_box_find_child(proj->child_boxes, GF_ISOM_BOX_TYPE_EQUI);
			if (projt) {
				info->projection_type = 3;
			}
		}
	}
	return GF_OK;
}


GF_EXPORT
u32 gf_isom_get_chunk_count(GF_ISOFile *movie, u32 trackNumber)
{
	GF_ChunkOffsetBox *stco;
	GF_TrackBox *trak;
	if (!movie || !movie->moov || !trackNumber) return 0;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->ChunkOffset ) return 0;

	stco = (GF_ChunkOffsetBox *) trak->Media->information->sampleTable->ChunkOffset;
	if (stco->type == GF_ISOM_BOX_TYPE_STCO)
		return stco->nb_entries;
	if (stco->type == GF_ISOM_BOX_TYPE_CO64)
		return ((GF_ChunkLargeOffsetBox *) stco)->nb_entries;
	return 0;
}

GF_EXPORT
GF_Err gf_isom_get_chunk_info(GF_ISOFile *movie, u32 trackNumber, u32 chunk_num, u64 *chunk_offset, u32 *first_sample_num, u32 *sample_per_chunk, u32 *sample_desc_idx, u32 *cache_1, u32 *cache_2)
{
	GF_ChunkOffsetBox *stco = NULL;
	GF_ChunkLargeOffsetBox *co64 = NULL;
	GF_SampleToChunkBox *stsc = NULL;
	GF_TrackBox *trak;
	u32 i, nb_entries, nb_samples, sample_desc_index;
	if (!movie || !movie->moov || !trackNumber || !chunk_num) return GF_BAD_PARAM;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak || !trak->Media || !trak->Media->information || !trak->Media->information->sampleTable || !trak->Media->information->sampleTable->ChunkOffset ) return GF_BAD_PARAM;

	stsc = (GF_SampleToChunkBox *) trak->Media->information->sampleTable->SampleToChunk;
	stco = (GF_ChunkOffsetBox *) trak->Media->information->sampleTable->ChunkOffset;
	if (stco->type == GF_ISOM_BOX_TYPE_CO64) {
		stco = NULL;
		co64 = (GF_ChunkLargeOffsetBox *) trak->Media->information->sampleTable->ChunkOffset;
		nb_entries = co64->nb_entries;
	} else {
		nb_entries = stco->nb_entries;
	}
	if (chunk_num>nb_entries) return GF_BAD_PARAM;

	sample_desc_index = 0;
	nb_samples = 1;
	i=0;

	if (cache_1 && cache_2) {
		if (chunk_num==1) {
			*cache_1 = 0;
			*cache_2 = 1;
		}
		i = *cache_1;
		nb_samples = *cache_2;
	}

	for (; i<stsc->nb_entries; i++) {
		u32 nb_chunks_before;

		if (stsc->entries[i].firstChunk == chunk_num) {
			sample_desc_index = stsc->entries[i].sampleDescriptionIndex;
			if (sample_per_chunk)
				*sample_per_chunk = stsc->entries[i].samplesPerChunk;
			break;
		}
		assert(stsc->entries[i].firstChunk<chunk_num);

		if ((i+1 == stsc->nb_entries)
			|| (stsc->entries[i+1].firstChunk>chunk_num)
		) {
			nb_chunks_before = chunk_num - stsc->entries[i].firstChunk;
			nb_samples += stsc->entries[i].samplesPerChunk * nb_chunks_before;
			sample_desc_index = stsc->entries[i].sampleDescriptionIndex;
			if (sample_per_chunk)
				*sample_per_chunk = stsc->entries[i].samplesPerChunk;
			break;
		}
		assert(stsc->entries[i+1].firstChunk > stsc->entries[i].firstChunk);

		nb_chunks_before = stsc->entries[i+1].firstChunk - stsc->entries[i].firstChunk;
		nb_samples += stsc->entries[i].samplesPerChunk * nb_chunks_before;

		if (cache_1 && cache_2) {
			*cache_1 = i+1;
			*cache_2 = nb_samples;
		}
	}

	if (first_sample_num) *first_sample_num = nb_samples;
	if (sample_desc_idx) *sample_desc_idx = sample_desc_index;
	if (chunk_offset) {
		if (stco)
			*chunk_offset = stco->offsets[chunk_num-1];
		else
			*chunk_offset = co64->offsets[chunk_num-1];
	}
	return GF_OK;
}

GF_EXPORT
GF_Err gf_isom_get_clean_aperture(GF_ISOFile *movie, u32 trackNumber, u32 StreamDescriptionIndex, u32 *cleanApertureWidthN, u32 *cleanApertureWidthD, u32 *cleanApertureHeightN, u32 *cleanApertureHeightD, s32 *horizOffN, u32 *horizOffD, s32 *vertOffN, u32 *vertOffD)
{
	GF_TrackBox *trak;
	GF_SampleEntryBox *entry;
	GF_SampleDescriptionBox *stsd;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return GF_BAD_PARAM;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return movie->LastError = GF_ISOM_INVALID_FILE;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return movie->LastError = GF_BAD_PARAM;
	}
	entry = (GF_SampleEntryBox *)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	if (entry == NULL) return GF_BAD_PARAM;
	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return GF_BAD_PARAM;

	GF_CleanApertureBox *clap = (GF_CleanApertureBox *)gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_CLAP);

	if (cleanApertureWidthN) *cleanApertureWidthN = clap ? clap->cleanApertureWidthN : 0;
	if (cleanApertureWidthD) *cleanApertureWidthD = clap ? clap->cleanApertureWidthD : 0;
	if (cleanApertureHeightN) *cleanApertureHeightN = clap ? clap->cleanApertureHeightN : 0;
	if (cleanApertureHeightD) *cleanApertureHeightD = clap ? clap->cleanApertureHeightD : 0;
	if (horizOffN) *horizOffN = clap ? clap->horizOffN : 0;
	if (horizOffD) *horizOffD = clap ? clap->horizOffD : 0;
	if (vertOffN) *vertOffN = clap ? clap->vertOffN : 0;
	if (vertOffD) *vertOffD = clap ? clap->vertOffD : 0;
	return GF_OK;
}

GF_EXPORT
u32 gf_isom_get_track_group(GF_ISOFile *file, u32 track_number, u32 track_group_type)
{
	u32 i;
	GF_TrackGroupTypeBox *trgt;
	GF_TrackBox *trak;

	trak = gf_isom_get_track_from_file(file, track_number);
	if (!trak) return 0;
	if (!trak->groups) return 0;

	for (i=0; i<gf_list_count(trak->groups->groups); i++) {
		trgt = gf_list_get(trak->groups->groups, i);
		if (trgt->group_type == track_group_type) {
			return trgt->track_group_id;
		}
	}
	return 0;
}

GF_EXPORT
const GF_MasteringDisplayColourVolumeInfo *gf_isom_get_mastering_display_colour_info(GF_ISOFile* movie, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_TrackBox* trak;
	GF_SampleEntryBox* entry;
	GF_SampleDescriptionBox* stsd;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return NULL;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return NULL;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return NULL;
	}
	entry = (GF_SampleEntryBox*)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	if (entry == NULL) return NULL;
	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return NULL;

	GF_MasteringDisplayColourVolumeBox *mdcvb = (GF_MasteringDisplayColourVolumeBox *) gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_MDCV);
	if (!mdcvb) return NULL;

	return &mdcvb->mdcv;
}

GF_EXPORT
const GF_ContentLightLevelInfo *gf_isom_get_content_light_level_info(GF_ISOFile* movie, u32 trackNumber, u32 StreamDescriptionIndex)
{
	GF_TrackBox* trak;
	GF_SampleEntryBox* entry;
	GF_SampleDescriptionBox* stsd;

	trak = gf_isom_get_track_from_file(movie, trackNumber);
	if (!trak) return NULL;

	stsd = trak->Media->information->sampleTable->SampleDescription;
	if (!stsd) return NULL;
	if (!StreamDescriptionIndex || StreamDescriptionIndex > gf_list_count(stsd->child_boxes)) {
		return NULL;
	}
	entry = (GF_SampleEntryBox*)gf_list_get(stsd->child_boxes, StreamDescriptionIndex - 1);
	if (entry == NULL) return NULL;
	if (entry->internal_type != GF_ISOM_SAMPLE_ENTRY_VIDEO) return NULL;

	GF_ContentLightLevelBox *cllib = (GF_ContentLightLevelBox *)gf_isom_box_find_child(entry->child_boxes, GF_ISOM_BOX_TYPE_CLLI);
	if (!cllib) return NULL;
	return &cllib->clli;
}


GF_Err gf_isom_enum_sample_aux_data(GF_ISOFile *the_file, u32 trackNumber, u32 sample_number, u32 *sai_idx, u32 *sai_type, u32 *sai_parameter, u8 **sai_data, u32 *sai_size)
{
	GF_TrackBox *trak;
	u32 i, count;

	if (!sai_type || !sai_idx || !sai_data || !sai_size) return GF_BAD_PARAM;
	if (sai_parameter) *sai_parameter = 0;
	*sai_type = 0;

	trak = gf_isom_get_track_from_file(the_file, trackNumber);
	if (!trak) return GF_BAD_PARAM;
	if (!trak->Media->information->sampleTable->sai_sizes) return GF_OK;
	if (!trak->Media->information->sampleTable->sai_offsets) return GF_OK;

#ifndef	GPAC_DISABLE_ISOM_FRAGMENTS
	if (sample_number <= trak->sample_count_at_seg_start) return GF_BAD_PARAM;
	sample_number -= trak->sample_count_at_seg_start;
#endif

	count = gf_list_count(trak->Media->information->sampleTable->sai_sizes);
	for (i=0; i<count; i++) {
		GF_Err e;
		GF_SampleAuxiliaryInfoSizeBox *saiz;
		GF_SampleAuxiliaryInfoOffsetBox *saio=NULL;
		u32 j;
		saiz = (GF_SampleAuxiliaryInfoSizeBox*)gf_list_get(trak->Media->information->sampleTable->sai_sizes, i);

		switch (saiz->aux_info_type) {
		case GF_ISOM_CENC_SCHEME:
		case GF_ISOM_CBC_SCHEME:
		case GF_ISOM_CENS_SCHEME:
		case GF_ISOM_CBCS_SCHEME:
		case GF_ISOM_PIFF_SCHEME:
		case 0:
			continue;
		default:
			break;
		}
		if (*sai_idx>i) continue;

		for (j=0; j<gf_list_count(trak->Media->information->sampleTable->sai_offsets); j++) {
			saio = (GF_SampleAuxiliaryInfoOffsetBox*)gf_list_get(trak->Media->information->sampleTable->sai_offsets, j);
			if ((saio->aux_info_type == saiz->aux_info_type) && (saio->aux_info_type_parameter == saiz->aux_info_type_parameter)) break;
			saio = NULL;
		}
		if (!saio) continue;
		if (!saio->offsets && !saio->sai_data) continue;

		u64 offset = saio->offsets ? saio->offsets[0] : 0;
		u32 nb_saio = saio->entry_count;
		if ((nb_saio>1) && (saio->entry_count != saiz->sample_count)) continue;

		*sai_type = saiz->aux_info_type;
		if (sai_parameter) *sai_parameter = saiz->aux_info_type_parameter;

		(*sai_idx)++;

		if (nb_saio == 1) {
			for (j=0; j < sample_number-1; j++) {
				u32 size = saiz->default_sample_info_size ? saiz->default_sample_info_size : saiz->sample_info_size[j];
				offset += size;
			}
		} else {
			offset = saio->offsets[sample_number-1];
		}

		*sai_size = saiz->default_sample_info_size ? saiz->default_sample_info_size : saiz->sample_info_size[j];
		if (*sai_size) {
			*sai_data = gf_malloc( *sai_size);
			if (! *sai_data) return GF_OUT_OF_MEM;
		}

		e = GF_OK;
		if (saio->sai_data) {
			if (offset + *sai_size <= saio->sai_data->dataSize) {
				memcpy(*sai_data, saio->sai_data->data + offset, *sai_size);
			} else {
				e = GF_IO_ERR;
			}
		} else {
			u64 cur_position = gf_bs_get_position(the_file->movieFileMap->bs);
			gf_bs_seek(the_file->movieFileMap->bs, offset);

			u32 nb_read = gf_bs_read_data(the_file->movieFileMap->bs, *sai_data, *sai_size);
			if (nb_read != *sai_size) e = GF_IO_ERR;
			gf_bs_seek(the_file->movieFileMap->bs, cur_position);
		}

		if (e) {
			gf_free(*sai_data);
			*sai_data = NULL;
			*sai_size = 0;
			GF_LOG(GF_LOG_ERROR, GF_LOG_CONTAINER, ("[isobmf] Failed to clone sai data: %s\n", gf_error_to_string(e) ));
		}
		return e;
	}
	return GF_OK;
}

GF_Err gf_isom_pop_emsg(GF_ISOFile *the_file, u8 **emsg_data, u32 *emsg_size)
{
#ifndef GPAC_DISABLE_ISOM_FRAGMENTS
	GF_Box *emsg = gf_list_pop_front(the_file->emsgs);
	if (!emsg) return GF_NOT_FOUND;

	GF_BitStream *bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
	//write everything
	gf_isom_box_size(emsg);
	gf_isom_box_write(emsg, bs);
	gf_bs_get_content(bs, emsg_data, emsg_size);
	gf_isom_box_del(emsg);
	return GF_OK;

#else
	return GF_NOT_SUPPORTED;
#endif

}

#endif /*GPAC_DISABLE_ISOM*/
