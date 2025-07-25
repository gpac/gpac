/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2024
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


/*File Mapping object, read-only mode on complete files (no download)*/
//#define GF_ISOM_DATA_FILE_MAPPING 0x02

#ifndef GPAC_DISABLE_ISOM

GF_BitStream *gf_bs_from_fd(int fd, u32 mode);

void gf_isom_datamap_del(GF_DataMap *ptr)
{
	if (!ptr) return;

	if (ptr->szName) gf_free(ptr->szName);

	//then delete the structure itself....
	switch (ptr->type) {
	//file-based
	case GF_ISOM_DATA_FILE:
	case GF_ISOM_DATA_MEM:
		gf_isom_fdm_del((GF_FileDataMap *)ptr);
		break;
#if 0
	case GF_ISOM_DATA_FILE_MAPPING:
		gf_isom_fmo_del((GF_FileMappingDataMap *)ptr);
		break;
#endif
	default:
		if (ptr->bs) gf_bs_del(ptr->bs);
		gf_free(ptr);
		break;
	}
}

//Close a data entry
void gf_isom_datamap_close(GF_MediaInformationBox *minf)
{
	GF_DataEntryBox *ent = NULL;
	if (!minf || !minf->dataHandler) return;

	if (minf->dataInformation && minf->dataInformation->dref) {
		ent = (GF_DataEntryBox*)gf_list_get(minf->dataInformation->dref->child_boxes, minf->dataEntryIndex - 1);
	}

	//if ent NULL, the data entry was not used (smooth)
	if (ent == NULL) return;

	//self contained, do nothing
	switch (ent->type) {
	case GF_ISOM_BOX_TYPE_URL:
	case GF_ISOM_BOX_TYPE_URN:
		if (ent->flags == 1) return;
		break;
	default:
		return;
	}

	//finally close it
	gf_isom_datamap_del(minf->dataHandler);
	minf->dataHandler = NULL;
}

/*cf below, we disable filedatamap since it tricks mem usage on w32*/
#if 0
static Bool IsLargeFile(char *path)
{
#ifndef _WIN32_WCE
	FILE *stream;
	s64 size;
	stream = gf_fopen(path, "rb");
	if (!stream) return 0;
	size = gf_fsize(stream);
	gf_fclose(stream);
	if (size == -1L) return 0;
	if (size > 0xFFFFFFFF) return 1;
#endif
	return 0;
}
#endif


//Special constructor, we need some error feedback...

GF_Err gf_isom_datamap_new(const char *location, const char *parentPath, u8 mode, GF_DataMap **outDataMap)
{
	Bool extern_file;
	char *sPath;
	*outDataMap = NULL;

	//if nothing specified, this is a MEMORY data map
	if (!location) {
		*outDataMap = gf_isom_fdm_new(location, GF_ISOM_DATA_MAP_WRITE);
		if (!(*outDataMap)) {
			return GF_IO_ERR;
		}
		return GF_OK;
	}
	//we need a temp file ...
	if (!strcmp(location, "_gpac_isobmff_tmp_edit")) {
#ifndef GPAC_DISABLE_ISOM_WRITE
		*outDataMap = gf_isom_fdm_new_temp(parentPath);
		if (! (*outDataMap)) {
			return GF_IO_ERR;
		}
		return GF_OK;
#else
		return GF_NOT_SUPPORTED;
#endif
	} else if (!strncmp(location, "gmem://", 7) || !strncmp(location, "gfio://", 7)) {
		*outDataMap = gf_isom_fdm_new(location, GF_ISOM_DATA_MAP_READ);
		if (! (*outDataMap)) {
			return GF_IO_ERR;
		}
		return GF_OK;
	} else if (!strcmp(location, "_gpac_isobmff_redirect")) {
		*outDataMap = gf_isom_fdm_new(location, mode);
		if (! (*outDataMap)) {
			return GF_IO_ERR;
		}
		return GF_OK;
	}

	extern_file = !gf_url_is_local(location);

	if (mode == GF_ISOM_DATA_MAP_EDIT) {
		//we need a local file for edition!!!
		if (extern_file) return GF_ISOM_INVALID_MODE;
		//OK, switch back to READ mode
		mode = GF_ISOM_DATA_MAP_READ;
	}

	//TEMP: however, only support for file right now (we'd have to add some callback functions at some point)
	if (extern_file) {
		return GF_NOT_SUPPORTED;
	}

	sPath = gf_url_concatenate(parentPath, location);
	if (sPath == NULL) {
		return GF_URL_ERROR;
	}

	if (mode == GF_ISOM_DATA_MAP_READ_ONLY) {
		mode = GF_ISOM_DATA_MAP_READ;

#if 0 //file mapping is disabled
		if (IsLargeFile(sPath)) {
			*outDataMap = gf_isom_fdm_new(sPath, mode);
		} else {
			*outDataMap = gf_isom_fmo_new(sPath, mode);
		}
#else
		*outDataMap = gf_isom_fdm_new(sPath, mode);
#endif
	} else {
		*outDataMap = gf_isom_fdm_new(sPath, mode);
		if (*outDataMap) {
			(*outDataMap)->szName = sPath;
			sPath = NULL;
		}
	}

	if (sPath) {
		gf_free(sPath);
	}
	if (! (*outDataMap)) {
		return GF_URL_ERROR;
	}

	return GF_OK;
}

//Open a data entry of a track
//Edit is used to switch between original and edition file
GF_Err gf_isom_datamap_open(GF_MediaBox *mdia, u32 dataRefIndex, u8 Edit)
{
	GF_DataEntryBox *ent;
	GF_MediaInformationBox *minf;
	u32 SelfCont, count;
	GF_Err e = GF_OK;
	if ((mdia == NULL) || (! mdia->information) || !dataRefIndex)
		return GF_ISOM_INVALID_MEDIA;

	minf = mdia->information;
	if (!minf->dataInformation || !minf->dataInformation->dref)
		return GF_ISOM_INVALID_MEDIA;

	count = gf_list_count(minf->dataInformation->dref->child_boxes);
	if (!count) {
		SelfCont = 1;
		ent = NULL;
	} else {
		if (dataRefIndex > gf_list_count(minf->dataInformation->dref->child_boxes))
			return GF_BAD_PARAM;

		ent = (GF_DataEntryBox*)gf_list_get(minf->dataInformation->dref->child_boxes, dataRefIndex - 1);
		if (ent == NULL) return GF_ISOM_INVALID_MEDIA;

		//if the current dataEntry is the desired one, and not self contained, return
		if ((minf->dataEntryIndex == dataRefIndex) && (ent->flags != 1)) {
			return GF_OK;
		}

		SelfCont = 0;
		switch (ent->type) {
		case GF_ISOM_BOX_TYPE_URL:
		case GF_ISOM_BOX_TYPE_URN:
			if (ent->flags == 1) SelfCont = 1;
			break;
		default:
			SelfCont = 1;
			break;
		}
	}

	//we need to open a new one
	//first close the existing one
	if (minf->dataHandler) gf_isom_datamap_close(minf);

	//if self-contained, assign the input file
	if (SelfCont) {
		//if no edit, open the input file
		if (!Edit) {
			if (mdia->mediaTrack->moov->mov->movieFileMap == NULL) return GF_ISOM_INVALID_FILE;
			minf->dataHandler = mdia->mediaTrack->moov->mov->movieFileMap;
		} else {
#ifndef GPAC_DISABLE_ISOM_WRITE
			if (mdia->mediaTrack->moov->mov->editFileMap == NULL) return GF_ISOM_INVALID_FILE;
			minf->dataHandler = mdia->mediaTrack->moov->mov->editFileMap;
#else
			//this should never be the case in an read-only MP4 file
			return GF_BAD_PARAM;
#endif
		}
		//else this is a URL (read mode only)
	} else {
#ifndef GPAC_DISABLE_ISOM_WRITE
		e = gf_isom_datamap_new(ent->location, mdia->mediaTrack->moov->mov->fileName ? mdia->mediaTrack->moov->mov->fileName : mdia->mediaTrack->moov->mov->finalName, GF_ISOM_DATA_MAP_READ, & mdia->information->dataHandler);
#else
		e = gf_isom_datamap_new(ent->location, mdia->mediaTrack->moov->mov->fileName, GF_ISOM_DATA_MAP_READ, & mdia->information->dataHandler);
#endif
		if (e) return (e==GF_URL_ERROR) ? GF_ISOM_UNKNOWN_DATA_REF : e;
	}
	//OK, set the data entry index
	minf->dataEntryIndex = dataRefIndex;
	return GF_OK;
}

//return the NB of bytes actually read (used for HTTP, ...) in case file is uncomplete
u32 gf_isom_datamap_get_data(GF_DataMap *map, u8 *buffer, u32 bufferLength, u64 Offset, GF_BlobRangeStatus *is_corrupted)
{
	if (!map || !buffer) return 0;

	switch (map->type) {
	case GF_ISOM_DATA_FILE:
	case GF_ISOM_DATA_MEM:
		return gf_isom_fdm_get_data((GF_FileDataMap *)map, buffer, bufferLength, Offset, is_corrupted);

#if 0
	case GF_ISOM_DATA_FILE_MAPPING:
		return gf_isom_fmo_get_data((GF_FileMappingDataMap *)map, buffer, bufferLength, Offset, is_corrupted);
#endif

	default:
		return 0;
	}
}


void gf_isom_datamap_flush(GF_DataMap *map)
{
	if (!map) return;

	if (map->type == GF_ISOM_DATA_FILE || map->type == GF_ISOM_DATA_MEM) {
		GF_FileDataMap *fdm = (GF_FileDataMap *)map;
		gf_bs_flush(fdm->bs);
	}
}

#ifndef GPAC_DISABLE_ISOM_WRITE

u64 FDM_GetTotalOffset(GF_FileDataMap *ptr);
GF_Err FDM_AddData(GF_FileDataMap *ptr, char *data, u32 dataSize);

u64 gf_isom_datamap_get_offset(GF_DataMap *map)
{
	if (!map) return 0;

	switch (map->type) {
	case GF_ISOM_DATA_FILE:
		return FDM_GetTotalOffset((GF_FileDataMap *)map);
	case GF_ISOM_DATA_MEM:
		return gf_bs_get_position(map->bs);
	default:
		return 0;
	}
}


GF_Err gf_isom_datamap_add_data(GF_DataMap *ptr, u8 *data, u32 dataSize)
{
	if (!ptr || !data|| !dataSize) return GF_BAD_PARAM;

	switch (ptr->type) {
	case GF_ISOM_DATA_FILE:
	case GF_ISOM_DATA_MEM:
		return FDM_AddData((GF_FileDataMap *)ptr, data, dataSize);
	default:
		return GF_NOT_SUPPORTED;
	}
}

GF_DataMap *gf_isom_fdm_new_temp(const char *sPath)
{
	GF_FileDataMap *tmp;
	GF_SAFEALLOC(tmp, GF_FileDataMap);
	if (!tmp) return NULL;

	tmp->type = GF_ISOM_DATA_FILE;
	tmp->mode = GF_ISOM_DATA_MAP_WRITE;
#ifdef GPAC_HAS_FD
	tmp->fd = -1;
#endif

	if (!sPath) {
		tmp->stream = gf_file_temp(&tmp->temp_file);
	} else {
		char szPath[GF_MAX_PATH];
		if (strlen(sPath) && (sPath[strlen(sPath)-1] != '\\') && (sPath[strlen(sPath)-1] != '/')) {
			sprintf(szPath, "%s%c%p_isotmp", sPath, GF_PATH_SEPARATOR, (void*) tmp);
		} else {
			sprintf(szPath, "%s%p_isotmp", sPath, (void*) tmp);
		}
		tmp->stream = gf_fopen(szPath, "w+b");
		tmp->temp_file = gf_strdup(szPath);
	}
	if (!tmp->stream) {
		if (tmp->temp_file) gf_free(tmp->temp_file);
		gf_free(tmp);
		return NULL;
	}
	tmp->bs = gf_bs_from_file(tmp->stream, GF_BITSTREAM_WRITE);
	if (!tmp->bs) {
		gf_fclose(tmp->stream);
		gf_free(tmp);
		return NULL;
	}
	return (GF_DataMap *)tmp;
}

#endif /*GPAC_DISABLE_ISOM_WRITE*/

#include <errno.h>
#include <string.h>
GF_DataMap *gf_isom_fdm_new(const char *sPath, u8 mode)
{
	u8 bs_mode = GF_BITSTREAM_READ;
	GF_FileDataMap *tmp;
	GF_SAFEALLOC(tmp, GF_FileDataMap);
	if (!tmp) return NULL;

	tmp->mode = mode;
#ifdef GPAC_HAS_FD
	tmp->fd = -1;
#endif

	if (sPath == NULL) {
		tmp->type = GF_ISOM_DATA_MEM;
		tmp->bs = gf_bs_new (NULL, 0, GF_BITSTREAM_WRITE);
		if (!tmp->bs) {
			gf_free(tmp);
			return NULL;
		}
		return (GF_DataMap *)tmp;
	}

	tmp->type = GF_ISOM_DATA_FILE;
#ifndef GPAC_DISABLE_ISOM_WRITE
	//open a temp file
	if (!strcmp(sPath, "_gpac_isobmff_tmp_edit")) {
		//create a temp file (that only occurs in EDIT/WRITE mode)
		tmp->stream = gf_file_temp(&tmp->temp_file);
//		bs_mode = GF_BITSTREAM_READ;
	}
#endif
	if (!strncmp(sPath, "gmem://", 7)) {
		if (sscanf(sPath, "gmem://%p", &tmp->blob) != 1)
			return NULL;
		gf_mx_p(tmp->blob->mx);
		tmp->bs = gf_bs_new(tmp->blob->data, tmp->blob->size, GF_BITSTREAM_READ);
		gf_mx_v(tmp->blob->mx);
		if (!tmp->bs) {
			gf_free(tmp);
			return NULL;
		}
		tmp->use_blob = 1;
		return (GF_DataMap *)tmp;
	}

	switch (mode) {
	case GF_ISOM_DATA_MAP_READ:
#ifdef GPAC_HAS_FD
		if (strncmp(sPath, "gfio://", 7) && !gf_opts_get_bool("core", "no-fd")) {
			tmp->fd = gf_fd_open(sPath, O_RDONLY | O_BINARY, S_IRUSR | S_IWUSR);
			if (tmp->fd<0) break;
			tmp->bs = gf_bs_from_fd(tmp->fd, GF_BITSTREAM_READ);
		} else
#endif
			if (!tmp->stream) tmp->stream = gf_fopen(sPath, "rb");

		bs_mode = GF_BITSTREAM_READ;
		break;
	///we open the file in READ/WRITE mode, in case
	case GF_ISOM_DATA_MAP_WRITE:
		if (!strcmp(sPath, "_gpac_isobmff_redirect")) {
			tmp->stream = NULL;
			tmp->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		} else {
			if (!strcmp(sPath, "std")) {
				tmp->stream = stdout;
				tmp->is_stdout = 1;
			}
#ifdef GPAC_HAS_FD
			if (strncmp(sPath, "gfio://", 7) && !gf_opts_get_bool("core", "no-fd")) {
				//make sure output dir exists
				gf_fopen(sPath, "mkdir");
				tmp->fd = gf_fd_open(sPath, O_RDWR | O_CREAT | O_TRUNC | O_BINARY, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH );
				if (tmp->fd<0) break;
				tmp->bs = gf_bs_from_fd(tmp->fd, GF_BITSTREAM_WRITE);
			} else
#endif
			if (!tmp->stream) {
				tmp->stream = gf_fopen(sPath, "w+b");
				if (!tmp->stream) tmp->stream = gf_fopen(sPath, "wb");
			}
		}
		bs_mode = GF_BITSTREAM_WRITE;
		break;
	///we open the file in CAT mode, in case
	case GF_ISOM_DATA_MAP_CAT:
		if (!strcmp(sPath, "_gpac_isobmff_redirect")) {
			tmp->stream = NULL;
			tmp->bs = gf_bs_new(NULL, 0, GF_BITSTREAM_WRITE);
		} else {
			if (!strcmp(sPath, "std")) {
				tmp->stream = stdout;
				tmp->is_stdout = 1;
			}

			if (!tmp->stream) tmp->stream = gf_fopen(sPath, "a+b");
			if (tmp->stream) gf_fseek(tmp->stream, 0, SEEK_END);
		}
		bs_mode = GF_BITSTREAM_WRITE;
		break;
	default:
		gf_free(tmp);
		return NULL;
	}
	if (!tmp->stream && !tmp->bs) {
		gf_free(tmp);
		return NULL;
	}
	if (!tmp->bs)
		tmp->bs = gf_bs_from_file(tmp->stream, bs_mode);

	if (!tmp->bs) {
		gf_fclose(tmp->stream);
		gf_free(tmp);
		return NULL;
	}
	return (GF_DataMap *)tmp;
}

void gf_isom_fdm_del(GF_FileDataMap *ptr)
{
	if (!ptr || (ptr->type != GF_ISOM_DATA_FILE && ptr->type != GF_ISOM_DATA_MEM)) return;
	if (ptr->bs) gf_bs_del(ptr->bs);
	if (ptr->stream && !ptr->is_stdout)
		gf_fclose(ptr->stream);

#ifdef GPAC_HAS_FD
	if (ptr->fd>=0)
		close(ptr->fd);
#endif

#ifndef GPAC_DISABLE_ISOM_WRITE
	if (ptr->temp_file) {
		gf_file_delete(ptr->temp_file);
		gf_free(ptr->temp_file);
	}
#endif
	gf_free(ptr);
}

u32 gf_isom_fdm_get_data(GF_FileDataMap *ptr, u8 *buffer, u32 bufferLength, u64 fileOffset, GF_BlobRangeStatus *is_corrupted)
{
	u32 bytesRead;

	//can we seek till that point ???
	if (fileOffset > gf_bs_get_size(ptr->bs))
		return 0;

	if (is_corrupted)
		*is_corrupted = GF_BLOB_RANGE_VALID;

	if (ptr->blob) {
		gf_mx_p(ptr->blob->mx);
		GF_BlobRangeStatus rs = gf_blob_query_range(ptr->blob, fileOffset, bufferLength);
		if (is_corrupted) *is_corrupted = rs;
		if (rs==GF_BLOB_RANGE_IN_TRANSFER) {
			gf_mx_v(ptr->blob->mx);
			return 0;
		}
		gf_bs_reassign_buffer(ptr->bs, ptr->blob->data, ptr->blob->size);
		if (gf_bs_seek(ptr->bs, fileOffset) != GF_OK) {
			gf_mx_v(ptr->blob->mx);
			return 0;
		}
	} else if (gf_bs_get_position(ptr->bs) != fileOffset) {
		//we are not at the previous location, do a seek
		if (gf_bs_seek(ptr->bs, fileOffset) != GF_OK) return 0;
	}
	ptr->curPos = fileOffset;

	//read our data.
	bytesRead = gf_bs_read_data(ptr->bs, buffer, bufferLength);
	//update our cache
	if (bytesRead == bufferLength) {
		ptr->curPos += bytesRead;
	} else if (!ptr->blob) {
		gf_bs_get_refreshed_size(ptr->bs);
		gf_bs_seek(ptr->bs, fileOffset);
		bytesRead = gf_bs_read_data(ptr->bs, buffer, bufferLength);
		//update our cache
		if (bytesRead == bufferLength) {
			ptr->curPos += bytesRead;
		} else {
			gf_bs_seek(ptr->bs, ptr->curPos);
			bytesRead = 0;
		}
	}
	ptr->last_acces_was_read = 1;

	if (ptr->blob) {
		gf_mx_v(ptr->blob->mx);
	}
	return bytesRead;
}

static Bool gf_isom_fdm_check_top_level(GF_FileDataMap *ptr)
{
	if (!ptr->blob) {
		if (!gf_bs_available(ptr->bs)) return GF_FALSE;
		return GF_TRUE;
	}
	//if we use a blob, make sure the top-level box is complete
	gf_mx_p(ptr->blob->mx);
	u64 fileOffset = gf_bs_get_position(ptr->bs);
	gf_bs_reassign_buffer(ptr->bs, ptr->blob->data, ptr->blob->size);
	if (ptr->blob->size < fileOffset+8) {
		gf_mx_v(ptr->blob->mx);
		return GF_FALSE;
	}
	gf_bs_seek(ptr->bs, fileOffset);
	u32 size = gf_bs_peek_bits(ptr->bs, 32, 0);
	//no size: either range is invalid or the box extends till end of blob
	if (!size)
		size = ptr->blob->size - (u32) fileOffset;
	GF_BlobRangeStatus rs = gf_blob_query_range(ptr->blob, fileOffset, size);
	gf_mx_v(ptr->blob->mx);
	if (rs==GF_BLOB_RANGE_IN_TRANSFER)
		return GF_FALSE;
	return GF_TRUE;
}

Bool gf_isom_datamap_top_level_box_avail(GF_DataMap *map)
{
	if (!map) return GF_FALSE;

	switch (map->type) {
	case GF_ISOM_DATA_FILE:
	case GF_ISOM_DATA_MEM:
		return gf_isom_fdm_check_top_level((GF_FileDataMap *)map);

#if 0
	case GF_ISOM_DATA_FILE_MAPPING:
		if (gf_bs_available( ((GF_FileMappingDataMap*)map)->bs)) return GF_TRUE;
		return GF_FALSE;
#endif

	default:
		return 0;
	}
}

#ifndef GPAC_DISABLE_ISOM_WRITE


u64 FDM_GetTotalOffset(GF_FileDataMap *ptr)
{
	if (!ptr) return 0;
	//the pos is not always at the end
	//this function is called to set up the chunks
	//so we need the next WRITE offset
	return gf_bs_get_size(ptr->bs);
}

GF_Err FDM_AddData(GF_FileDataMap *ptr, char *data, u32 dataSize)
{
	u32 ret;
	u64 orig;
	if (ptr->mode == GF_ISOM_DATA_MAP_READ) return GF_BAD_PARAM;

	orig = gf_bs_get_size(ptr->bs);

	/*last access was read, seek to end of file*/
	if (ptr->last_acces_was_read) {
		gf_bs_seek(ptr->bs, orig);
		ptr->last_acces_was_read = 0;
	}
	//OK, write our stuff to the datamap...
	//we don't use bs here cause we want to know more about what has been written
	ret = gf_bs_write_data(ptr->bs, data, dataSize);
	if (ret != dataSize) {
		ptr->curPos = orig;
		gf_bs_seek(ptr->bs, orig);
		return GF_IO_ERR;
	}
	ptr->curPos = gf_bs_get_position(ptr->bs);
#if 0
	//flush the stream !!
	if (ptr->stream) gf_bs_flush(ptr->bs);
#endif
	return GF_OK;
}

#endif	/*GPAC_DISABLE_ISOM_WRITE*/


#if 0 //file mapping disabled

#ifdef WIN32

#include <windows.h>
#include <winerror.h>

GF_DataMap *gf_isom_fmo_new(const char *sPath, u8 mode)
{
	GF_FileMappingDataMap *tmp;
	HANDLE fileH, fileMapH;
#ifdef _WIN32_WCE
	unsigned short sWPath[MAX_PATH];
#endif

	//only in read only
	if (mode != GF_ISOM_DATA_MAP_READ) return NULL;

	GF_SAFEALLOC(tmp, GF_FileMappingDataMap);
	if (!tmp) return NULL;

	tmp->type = GF_ISOM_DATA_FILE_MAPPING;
	tmp->mode = mode;
	tmp->name = gf_strdup(sPath);

	//
	//	Open the file
	//
#ifdef _WIN32_WCE
	//convert to WIDE
	CE_CharToWide((char *)sPath, sWPath);

	fileH = CreateFileForMapping(sWPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
	                             (FILE_ATTRIBUTE_READONLY | FILE_FLAG_RANDOM_ACCESS), NULL );
#else
	fileH = CreateFile(sPath, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING,
	                   (FILE_ATTRIBUTE_READONLY | FILE_FLAG_RANDOM_ACCESS), NULL );
#endif


	if (fileH == INVALID_HANDLE_VALUE) {
		gf_free(tmp->name);
		gf_free(tmp);
		return NULL;
	}

	tmp->file_size = GetFileSize(fileH, NULL);
	if (tmp->file_size == 0xFFFFFFFF) {
		CloseHandle(fileH);
		gf_free(tmp->name);
		gf_free(tmp);
		return NULL;
	}

	//
	//	Create the mapping
	//
	fileMapH = CreateFileMapping(fileH, NULL, PAGE_READONLY, 0, 0, NULL);
	if (fileMapH == NULL) {
		CloseHandle(fileH);
		gf_free(tmp->name);
		gf_free(tmp);
		return NULL;
	}

	tmp->byte_map = MapViewOfFile(fileMapH, FILE_MAP_READ, 0, 0, 0);
	if (tmp->byte_map == NULL) {
		CloseHandle(fileMapH);
		CloseHandle(fileH);
		gf_free(tmp->name);
		gf_free(tmp);
		return NULL;
	}

	CloseHandle(fileH);
	CloseHandle(fileMapH);

	//finaly open our bitstream (from buffer)
	tmp->bs = gf_bs_new(tmp->byte_map, tmp->file_size, GF_BITSTREAM_READ);
	return (GF_DataMap *)tmp;
}

void gf_isom_fmo_del(GF_FileMappingDataMap *ptr)
{
	if (!ptr || (ptr->type != GF_ISOM_DATA_FILE_MAPPING)) return;

	if (ptr->bs) gf_bs_del(ptr->bs);
	if (ptr->byte_map) UnmapViewOfFile(ptr->byte_map);
	gf_free(ptr->name);
	gf_free(ptr);
}


u32 gf_isom_fmo_get_data(GF_FileMappingDataMap *ptr, u8 *buffer, u32 bufferLength, u64 fileOffset, GF_BlobRangeStatus *is_corrupted)
{
	//can we seek till that point ???
	if (fileOffset > ptr->file_size) return 0;

	//we do only read operations, so trivial
	memcpy(buffer, ptr->byte_map + fileOffset, bufferLength);
	return bufferLength;
}

#else

GF_DataMap *gf_isom_fmo_new(const char *sPath, u8 mode)
{
	return gf_isom_fdm_new(sPath, mode);
}

void gf_isom_fmo_del(GF_FileMappingDataMap *ptr)
{
	gf_isom_fdm_del((GF_FileDataMap *)ptr);
}

u32 gf_isom_fmo_get_data(GF_FileMappingDataMap *ptr, u8 *buffer, u32 bufferLength, u64 fileOffset, GF_BlobRangeStatus *is_corrupted)
{
	return gf_isom_fdm_get_data((GF_FileDataMap *)ptr, buffer, bufferLength, fileOffset, is_corrupted);
}

#endif //win32
#endif //file mapping disabled

#endif /*GPAC_DISABLE_ISOM*/
