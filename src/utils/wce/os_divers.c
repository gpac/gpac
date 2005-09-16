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

#include <winsock.h>

void gf_sleep(u32 millisecs)
{
	Sleep(millisecs);
}

#define TIME_WRAP		(~(DWORD)0)

static u32 clock_init = 0;

static LARGE_INTEGER frequency;
static LARGE_INTEGER init_counter;


static u32 (*CE_GetSysClock)();

u32 gf_sys_clock()
{
	return CE_GetSysClock();
}

u32 CE_GetSysClockHIGHRES()
{
	LARGE_INTEGER now;
	u64 res;
	QueryPerformanceCounter(&now);

	now.QuadPart -= init_counter.QuadPart;
	res = now.QuadPart * 1000 / frequency.QuadPart;
	return (u32) res;
}

u32 CE_GetSysClockNORMAL()
{
	return GetTickCount();
}


void gf_sys_clock_start()
{
	if (!clock_init) {
		if (QueryPerformanceFrequency(&frequency) && 0) {
			QueryPerformanceCounter(&init_counter);
			CE_GetSysClock = CE_GetSysClockHIGHRES;
		} else {
			CE_GetSysClock = CE_GetSysClockNORMAL;
		}
	}
	clock_init += 1;
}

void gf_sys_clock_stop()
{
}

#ifndef gettimeofday

/*
	Conversion code for WinCE from pthreads WinCE
	(FILETIME in Win32 is from jan 1, 1601)
	time between jan 1, 1601 and jan 1, 1970 in units of 100 nanoseconds 
*/
#define TIMESPEC_TO_FILETIME_OFFSET (((LONGLONG)27111902 << 32) + (LONGLONG)3577643008)

s32 gettimeofday(struct timeval *tp, void *tz)
{
	FILETIME ft;
	SYSTEMTIME st;
	s32 val;

	GetSystemTime(&st);
    SystemTimeToFileTime(&st, &ft);

	val = (s32) ((*(LONGLONG *) &ft - TIMESPEC_TO_FILETIME_OFFSET) / 10000000);
	tp->tv_sec = (u32) val;
	val = (s32 ) ((*(LONGLONG *) &ft - TIMESPEC_TO_FILETIME_OFFSET - ((LONGLONG) val * (LONGLONG) 10000000)) * 100);
	tp->tv_usec = val;
	return 0;
}
#endif

void gf_utc_time_since_1970(u32 *sec, u32 *msec)
{
	struct timeval tv;
	gettimeofday(&tv, NULL);
	*sec = tv.tv_sec;
	*msec = tv.tv_usec/1000;
}


void gf_delete_file(char *fileName)
{
	TCHAR swzName[MAX_PATH];
	CE_CharToWide(fileName, swzName);
	DeleteFile(swzName);
}


FILE *gf_temp_file_new()
{
	TCHAR pPath[MAX_PATH+1];
	TCHAR pTemp[MAX_PATH+1];
	if (!GetTempPath(MAX_PATH, pPath)) {
		pPath[0] = '.';
		pPath[1] = '.';
	}
	if (GetTempFileName(pPath, TEXT("git"), 0, pTemp))
		return _wfopen(pTemp, TEXT("w+b"));

	return NULL;
}

void CE_Assert(u32 valid)
{
	if (!valid) {
		MessageBox(NULL, _T("ASSERT FAILED"), _T("Fatal Error"), MB_OK);
	}
}


void gf_rand_init(Bool Reset)
{
	if (Reset) {
		srand(1);
	} else {
		srand( (u32) GetTickCount() );
	}
}

u32 gf_rand()
{
	return rand();
}



void gf_get_user_name(char *buf, u32 buf_size)
{
	strcpy(buf, "mpeg4-user");

}


void CE_WideToChar(unsigned short *w_str, char *str)
{
	WideCharToMultiByte(CP_ACP, 0, w_str, -1, str, GF_MAX_PATH, NULL, NULL);
}

void CE_CharToWide(char *str, unsigned short *w_str)
{
	MultiByteToWideChar(CP_ACP, 0, str, -1, w_str, GF_MAX_PATH);
}



/*enumerate directories*/
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, Bool (*enum_dir_item)(void *cbck, char *item_name, char *item_path), void *cbck)
{
	unsigned char _path[GF_MAX_PATH];
	unsigned short path[GF_MAX_PATH];
	unsigned char file[GF_MAX_PATH], filepath[GF_MAX_PATH];
	WIN32_FIND_DATA FindData;
	HANDLE SearchH;

	if (!dir) return GF_BAD_PARAM;
	if (dir[strlen(dir) - 1] == GF_PATH_SEPARATOR) {
		sprintf(_path, "%s*", dir);
	} else {
		sprintf(_path, "%s%c*", dir, GF_PATH_SEPARATOR);
	}
	CE_CharToWide(_path, path);

	SearchH= FindFirstFile(path, &FindData);
	if (SearchH == INVALID_HANDLE_VALUE) return GF_IO_ERR;

	_path[strlen(_path)-1] = 0;

	while (SearchH != INVALID_HANDLE_VALUE) {
		if (!wcscmp(FindData.cFileName, _T(".") )) goto next;
		if (!wcscmp(FindData.cFileName, _T("..") )) goto next;

		if (!enum_directory && (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) goto next;
		if (enum_directory && !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) goto next;

		CE_WideToChar(FindData.cFileName, file);
		strcpy(filepath, _path);
		strcat(filepath, file);
		if (enum_dir_item(cbck, file, filepath)) {
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

u64 gf_f64_tell(FILE *f)
{
	return (u64) ftell(f);
}

u64 gf_f64_seek(FILE *f, s64 pos, s32 whence)
{
	return (u64) fseek(f, (s32) pos, whence);
}

FILE *gf_f64_open(const char *file_name, const char *mode)
{
	return fopen(file_name, mode);
}
