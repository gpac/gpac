/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
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

void gf_isom_datamap_del(GF_DataMap *ptr)
{
	if (!ptr) return;

	//then delete the structure itself....
	switch (ptr->type) {
	//file-based
	case GF_ISOM_DATA_FILE:
		gf_isom_fdm_del((GF_FileDataMap *)ptr);
		break;
	case GF_ISOM_DATA_FILE_MAPPING:
		gf_isom_fmo_del((GF_FileMappingDataMap *)ptr);
		break;
	//not implemented
	default:
		break;
	}
}

//Close a data entry
void gf_isom_datamap_close(GF_MediaInformationBox *minf)
{
	GF_DataEntryBox *ent;
	if (!minf || !minf->dataHandler) return;

	ent = (GF_DataEntryBox*)gf_list_get(minf->dataInformation->dref->boxList, minf->dataEntryIndex - 1);

	//if ent NULL, the data entry was not used (should never happen)
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
	stream = gf_f64_open(path, "rb");
	if (!stream) return 0;
	gf_f64_seek(stream, 0, SEEK_END);
	size = gf_f64_tell(stream);
	fclose(stream);
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
		//not supported yet
		return GF_NOT_SUPPORTED;
	}
	//we need a temp file ...
	if (!strcmp(location, "mp4_tmp_edit")) {
#ifndef GPAC_READ_ONLY
		*outDataMap = gf_isom_fdm_new_temp(parentPath);
		if (! (*outDataMap)) return GF_IO_ERR;
		return GF_OK;
#else
		return GF_NOT_SUPPORTED;
#endif
	}

	extern_file = !gf_url_is_local(location);

	if (mode == GF_ISOM_DATA_MAP_EDIT) {
		//we need a local file for edition!!!
		if (extern_file) return GF_ISOM_INVALID_MODE;
		//OK, switch back to READ mode
		mode = GF_ISOM_DATA_MAP_READ;
	}

	//TEMP: however, only support for file right now (we'd have to add some callback functions at some point)
	if (extern_file) return GF_NOT_SUPPORTED;

	sPath = gf_url_get_absolute_path(location, parentPath);
	if (sPath == NULL) return GF_URL_ERROR;

	if (mode == GF_ISOM_DATA_MAP_READ_ONLY) {
		mode = GF_ISOM_DATA_MAP_READ;
		/*It seems win32 file mapping is reported in prog mem usage -> large increases of occupancy. Should not be a pb 
		but unless you want mapping, only regular IO will be used...*/
#if 0
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
	}

	free(sPath);
	if (! (*outDataMap)) return GF_URL_ERROR;
	return GF_OK;
}

//Open a data entry of a track
//Edit is used to switch between original and edition file
GF_Err gf_isom_datamap_open(GF_MediaBox *mdia, u32 dataRefIndex, u8 Edit)
{
	GF_DataEntryBox *ent;
	GF_MediaInformationBox *minf;
	u32 SelfCont;
	GF_Err e = GF_OK;
	if ((mdia == NULL) || (! mdia->information) || !dataRefIndex)
		return GF_ISOM_INVALID_MEDIA;

	minf = mdia->information;

	if (dataRefIndex > gf_list_count(minf->dataInformation->dref->boxList))
		return GF_BAD_PARAM;

	ent = (GF_DataEntryBox*)gf_list_get(minf->dataInformation->dref->boxList, dataRefIndex - 1);
	if (ent == NULL) return GF_ISOM_INVALID_MEDIA;

	//if the current dataEntry is the desired one, and not self contained, return
	if ((minf->dataEntryIndex == dataRefIndex) && (ent->flags != 1)) {
		return GF_OK;
	}

	//we need to open a new one
	//first close the existing one
	if (minf->dataHandler) gf_isom_datamap_close(minf);

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
	//if self-contained, assign the input file
	if (SelfCont) {
		//if no edit, open the input file
		if (!Edit) {
			if (mdia->mediaTrack->moov->mov->movieFileMap == NULL) return GF_ISOM_INVALID_FILE;
			minf->dataHandler = mdia->mediaTrack->moov->mov->movieFileMap;
		} else {
#ifndef GPAC_READ_ONLY
			if (mdia->mediaTrack->moov->mov->editFileMap == NULL) return GF_ISOM_INVALID_FILE;
			minf->dataHandler = mdia->mediaTrack->moov->mov->editFileMap;
#else
			//this should never be the case in an read-only MP4 file
			return GF_BAD_PARAM;
#endif		
		}
	//else this is a URL (read mode only)
	} else {
		e = gf_isom_datamap_new(ent->location, mdia->mediaTrack->moov->mov->fileName, GF_ISOM_DATA_MAP_READ, & mdia->information->dataHandler);
		if (e) return (e==GF_URL_ERROR) ? GF_ISOM_UNKNOWN_DATA_REF : e;
	}
	//OK, set the data entry index
	minf->dataEntryIndex = dataRefIndex;
	return GF_OK;
}

//return the NB of bytes actually read (used for HTTP, ...) in case file is uncomplete
u32 gf_isom_datamap_get_data(GF_DataMap *map, char *buffer, u32 bufferLength, u64 Offset)
{
	if (!map || !buffer) return 0;

	switch (map->type) {
	case GF_ISOM_DATA_FILE:
		return gf_isom_fdm_get_data((GF_FileDataMap *)map, buffer, bufferLength, Offset);

	case GF_ISOM_DATA_FILE_MAPPING:
		return gf_isom_fmo_get_data((GF_FileMappingDataMap *)map, buffer, bufferLength, Offset);

	default:
		return 0;
	}
}


#ifndef GPAC_READ_ONLY

u64 FDM_GetTotalOffset(GF_FileDataMap *ptr);
GF_Err FDM_AddData(GF_FileDataMap *ptr, char *data, u32 dataSize);

u64 gf_isom_datamap_get_offset(GF_DataMap *map)
{
	if (!map) return 0;

	switch (map->type) {
	case GF_ISOM_DATA_FILE:
		return FDM_GetTotalOffset((GF_FileDataMap *)map);

	default:
		return 0;
	}
}


GF_Err gf_isom_datamap_add_data(GF_DataMap *ptr, char *data, u32 dataSize)
{
	if (!ptr || !data|| !dataSize) return GF_BAD_PARAM;

	switch (ptr->type) {
	case GF_ISOM_DATA_FILE:
		return FDM_AddData((GF_FileDataMap *)ptr, data, dataSize);
	default:
		return GF_NOT_SUPPORTED;
	}
}

#endif


#ifndef GPAC_READ_ONLY
GF_DataMap *gf_isom_fdm_new_temp(const char *sPath)
{
	GF_FileDataMap *tmp = (GF_FileDataMap *) malloc(sizeof(GF_FileDataMap));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_FileDataMap));
	tmp->type = GF_ISOM_DATA_FILE;
	tmp->mode = GF_ISOM_DATA_MAP_WRITE;

	if (!sPath) {
		tmp->stream = gf_temp_file_new();
	} else {
		char szPath[GF_MAX_PATH];
		if ((sPath[strlen(sPath)-1] != '\\') && (sPath[strlen(sPath)-1] != '/')) {
			sprintf(szPath, "%s%c%d_isotmp", sPath, GF_PATH_SEPARATOR, (u32) tmp);
		} else {
			sprintf(szPath, "%s%d_isotmp", sPath, (u32) tmp);
		}
		tmp->stream = gf_f64_open(szPath, "w+b");
		tmp->temp_file = strdup(szPath);
	}
	if (!tmp->stream) {
		if (tmp->temp_file) free(tmp->temp_file);
		free(tmp);
		return NULL;
	}
	tmp->bs = gf_bs_from_file(tmp->stream, GF_BITSTREAM_READ);
	if (!tmp->bs) {
		fclose(tmp->stream);
		free(tmp);
		return NULL;
	}
	return (GF_DataMap *)tmp;
}

#endif

GF_DataMap *gf_isom_fdm_new(const char *sPath, u8 mode)
{
	u8 bs_mode;

	GF_FileDataMap *tmp = (GF_FileDataMap *) malloc(sizeof(GF_FileDataMap));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_FileDataMap));
	tmp->type = GF_ISOM_DATA_FILE;
	tmp->mode = mode;

#ifndef GPAC_READ_ONLY
	//open a temp file
	if (!strcmp(sPath, "mp4_tmp_edit")) {
		//create  a temp file (that only occurs in EDIT/WRITE mode)
		tmp->stream = gf_temp_file_new();
		bs_mode = GF_BITSTREAM_READ;
	}
#endif

	switch (mode) {
	case GF_ISOM_DATA_MAP_READ:
		if (!tmp->stream) tmp->stream = gf_f64_open(sPath, "rb");
		bs_mode = GF_BITSTREAM_READ;
		break;
	///we open the file in READ/WRITE mode, in case 
	case GF_ISOM_DATA_MAP_WRITE:
		if (!tmp->stream) tmp->stream = gf_f64_open(sPath, "w+b");
		if (!tmp->stream) tmp->stream = gf_f64_open(sPath, "wb");
		bs_mode = GF_BITSTREAM_WRITE;
		break;
	default:
		free(tmp);
		return NULL;
	}
	if (!tmp->stream) {
		free(tmp);
		return NULL;
	}
	tmp->bs = gf_bs_from_file(tmp->stream, bs_mode);
	if (!tmp->bs) {
		fclose(tmp->stream);
		free(tmp);
		return NULL;
	}
	return (GF_DataMap *)tmp;
}

void gf_isom_fdm_del(GF_FileDataMap *ptr)
{
	if (!ptr || (ptr->type != GF_ISOM_DATA_FILE)) return;
	if (ptr->bs) gf_bs_del(ptr->bs);
	if (ptr->stream) fclose(ptr->stream);

#ifndef GPAC_READ_ONLY
	if (ptr->temp_file) {
		gf_delete_file(ptr->temp_file);
		free(ptr->temp_file);
	}
#endif
	free(ptr);
}



u32 gf_isom_fdm_get_data(GF_FileDataMap *ptr, char *buffer, u32 bufferLength, u64 fileOffset)
{
	u32 bytesRead;

	//can we seek till that point ???
	if (fileOffset > gf_bs_get_size(ptr->bs)) return 0;

	//ouch, we are not at the previous location, do a seek
	if (ptr->curPos != fileOffset) {
		if (gf_bs_seek(ptr->bs, fileOffset) != GF_OK) return 0;
		ptr->curPos = fileOffset;
	}
	//read our data.
	bytesRead = gf_bs_read_data(ptr->bs, buffer, bufferLength);
	//update our cache
	if (bytesRead == bufferLength) {
		ptr->curPos += bytesRead;
	} else {
		//rewind to original (if seek fails, return 0 cause this means:
		//1- no support for seek on the platform
		//2- corrupted file for the OS
		fflush(ptr->stream);
		gf_bs_seek(ptr->bs, ptr->curPos);
	}
	ptr->last_acces_was_read = 1;
	return bytesRead;
}



#ifndef GPAC_READ_ONLY


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
	//flush the stream !!
	fflush(ptr->stream);
	return GF_OK;
}

#endif	//GPAC_READ_ONLY


#ifdef WIN32

#include <windows.h>
#include <winerror.h>

GF_DataMap *gf_isom_fmo_new(const char *sPath, u8 mode)
{
	GF_FileMappingDataMap *tmp;
	HANDLE fileH, fileMapH;
	DWORD err;
#ifdef _WIN32_WCE
	unsigned short sWPath[MAX_PATH];
#endif

	//only in read only
	if (mode != GF_ISOM_DATA_MAP_READ) return NULL;

	tmp = (GF_FileMappingDataMap *) malloc(sizeof(GF_FileMappingDataMap));
	if (!tmp) return NULL;
	memset(tmp, 0, sizeof(GF_FileMappingDataMap));
	tmp->type = GF_ISOM_DATA_FILE_MAPPING;
	tmp->mode = mode;	
	tmp->name = _strdup(sPath);

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
		free(tmp->name);
		free(tmp);
		return NULL;
	}

	tmp->file_size = GetFileSize(fileH, NULL);
	if (tmp->file_size == 0xFFFFFFFF) {
		CloseHandle(fileH);
		free(tmp->name);
		free(tmp);
		return NULL;
	}

	//
	//	Create the mapping
	//
	fileMapH = CreateFileMapping(fileH, NULL, PAGE_READONLY, 0, 0, NULL);
	if (fileMapH == NULL) {
		CloseHandle(fileH);
		free(tmp->name);
		free(tmp);
		err = GetLastError();
		return NULL;
	}
		
	tmp->byte_map = MapViewOfFile(fileMapH, FILE_MAP_READ, 0, 0, 0);
	if (tmp->byte_map == NULL) {
		CloseHandle(fileMapH);
		CloseHandle(fileH);
		free(tmp->name);
		free(tmp);
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
	free(ptr->name);
	free(ptr);
}


u32 gf_isom_fmo_get_data(GF_FileMappingDataMap *ptr, char *buffer, u32 bufferLength, u64 fileOffset)
{
	u32 size;

	//can we seek till that point ???
	if (fileOffset > ptr->file_size) return 0;
	size = (u32) fileOffset;

	//we do only read operations, so trivial
	memcpy(buffer, ptr->byte_map + fileOffset, bufferLength);
	return bufferLength;
}

#else

GF_DataMap *gf_isom_fmo_new(const char *sPath, u8 mode) { return gf_isom_fdm_new(sPath, mode); }
void gf_isom_fmo_del(GF_FileMappingDataMap *ptr) { gf_isom_fdm_del((GF_FileDataMap *)ptr); }
u32 gf_isom_fmo_get_data(GF_FileMappingDataMap *ptr, char *buffer, u32 bufferLength, u64 fileOffset) 
{
	return gf_isom_fdm_get_data((GF_FileDataMap *)ptr, buffer, bufferLength, fileOffset);
}

#endif



