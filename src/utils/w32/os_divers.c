/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Copyright (c) Jean Le Feuvre 2000-2005
 *					All rights reserved
 *
 *  This file is part of GPAC / common tools sub-project
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

#include <gpac/tools.h>

#include <time.h>
#include <sys/timeb.h>
#include <io.h>
#include <windows.h>


void gf_sleep(u32 ms)
{
	Sleep(ms);
}

static u32 clock_init = 0;

/*TODO: add queryperformancetimer stuff for win32*/
void gf_sys_clock_start()
{
	if (!clock_init) timeBeginPeriod(1);
	clock_init += 1;
}

void gf_sys_clock_stop()
{
	if (clock_init > 0) {
		clock_init --;
		if (!clock_init) timeEndPeriod(1);
	}
}

u32 gf_sys_clock()
{
	return timeGetTime();
}

void gf_delete_file(char *fileName)
{
	DeleteFile(fileName);
}



void gf_rand_init(Bool Reset)
{
	if (Reset) {
		srand(1);
	} else {
		srand( (u32) time(NULL) );
	}
}

u32 gf_rand()
{
	return rand();
}


#ifndef GPAC_READ_ONLY
FILE *gf_temp_file_new()
{
	return tmpfile();
}
#endif


void gf_utc_time_since_1970(u32 *sec, u32 *msec)
{
	struct _timeb	tb;
	_ftime( &tb );
	*sec = tb.time;
	*msec = tb.millitm;

}

void gf_get_user_name(char *buf, u32 buf_size)
{
	strcpy(buf, "mpeg4-user");

#if 0

	s32 len;
	char *t;

	strcpy(buf, "");
	len = 1024;
	GetUserName(buf, &len);
	if (!len) {
		t = getenv("USER");
		if (t) strcpy(buf, t);
	}
#endif
}


/*enumerate directories*/
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, Bool (*enum_dir_item)(void *cbck, char *item_name, char *item_path), void *cbck)
{
	unsigned char path[GF_MAX_PATH], item_path[GF_MAX_PATH];
	WIN32_FIND_DATA FindData;
	HANDLE SearchH;

	if (!dir) return GF_BAD_PARAM;
	if (dir[strlen(dir) - 1] == GF_PATH_SEPARATOR) {
		sprintf(path, "%s*", dir);
	} else {
		sprintf(path, "%s%c*", dir, GF_PATH_SEPARATOR);
	}

	SearchH= FindFirstFile(path, &FindData);
	if (SearchH == INVALID_HANDLE_VALUE) return GF_IO_ERR;

	path[strlen(path)-1] = 0;

	while (SearchH != INVALID_HANDLE_VALUE) {
		if (!strcmp(FindData.cFileName, ".")) goto next;
		if (!strcmp(FindData.cFileName, "..")) goto next;

		if (!enum_directory && (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) goto next;
		if (enum_directory && !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) goto next;

		strcpy(item_path, path);
		strcat(item_path, FindData.cFileName);
		if (enum_dir_item(cbck, FindData.cFileName, item_path)) {
			FindClose(SearchH);
			break;
		}

next:
		if (!FindNextFile(SearchH, &FindData)) {
			FindClose(SearchH);
			break;
		}
	}
	return GF_OK;
}


u64 f64_tell(FILE *fp)
{
	fpos_t pos;
	if (fgetpos(fp, &pos))
		return (u64) -1;
	else
		return ((u64) pos);
}

u64 f64_seek(FILE *fp, s64 offset, s32 whence)
{
	fpos_t pos;
	if (whence == SEEK_CUR) {
		fgetpos(fp, &pos);
		pos += (fpos_t) offset;
	} 
	else if (whence == SEEK_END) 
		pos = (fpos_t) (_filelengthi64(fileno(fp)) + offset);
	else if (whence == SEEK_SET)
		pos = (fpos_t) offset;
	return fsetpos(fp, &pos);
}

FILE *f64_open(const char *file_name, const char *mode)
{
	return fopen(file_name, mode);
}
