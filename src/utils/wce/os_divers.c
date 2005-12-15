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
#include <tlhelp32.h>

void gf_sleep(u32 millisecs)
{
	Sleep(millisecs);
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
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, Bool (*enum_dir_item)(void *cbck, char *item_name, char *item_path), void *cbck, const char *filter)
{
	unsigned char _path[GF_MAX_PATH];
	unsigned short path[GF_MAX_PATH];
	unsigned short w_filter[GF_MAX_PATH];
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
	CE_CharToWide((char *)filter, w_filter);

	SearchH = FindFirstFile(path, &FindData);
	if (SearchH == INVALID_HANDLE_VALUE) return GF_IO_ERR;

	_path[strlen(_path)-1] = 0;

	while (SearchH != INVALID_HANDLE_VALUE) {
		if (!wcscmp(FindData.cFileName, _T(".") )) goto next;
		if (!wcscmp(FindData.cFileName, _T("..") )) goto next;

		if (!enum_directory && (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) goto next;
		if (enum_directory && !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) goto next;
		if (filter) {
			short ext[30];
			short *sep = wcsrchr(FindData.cFileName, (wchar_t) '.');
			if (!sep) goto next;
			wcscpy(ext, sep+1);
			wcslwr(ext);
			if (!wcsstr(w_filter, ext)) goto next;
		}

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





static u32 sys_init = 0;
static u32 last_update_time = 0;
static u64 last_total_k_u_time = 0;
static u64 last_process_k_u_time = 0;
static u32 mem_usage_at_startup = 0;
GF_SystemRTInfo the_rti;

static LARGE_INTEGER frequency;
static LARGE_INTEGER init_counter;
static u32 (*CE_GetSysClock)();

u32 gf_sys_clock() { return CE_GetSysClock(); }

u32 CE_GetSysClockHIGHRES()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	now.QuadPart -= init_counter.QuadPart;
	return (u32) ((now.QuadPart * 1000) / frequency.QuadPart);
}

u32 CE_GetSysClockNORMAL() { return GetTickCount(); }


void gf_sys_init()
{
	if (!sys_init) {
		MEMORYSTATUS ms;
		if (QueryPerformanceFrequency(&frequency) ) {
			QueryPerformanceCounter(&init_counter);
			CE_GetSysClock = CE_GetSysClockHIGHRES;
		} else {
			CE_GetSysClock = CE_GetSysClockNORMAL;
		}
		last_total_k_u_time = last_process_k_u_time = 0;
		last_update_time = 0;
		memset(&the_rti, 0, sizeof(GF_SystemRTInfo));
		the_rti.pid = GetCurrentProcessId();
		GlobalMemoryStatus(&ms);
		mem_usage_at_startup = ms.dwAvailPhys;
	}
	sys_init += 1;
}

void gf_sys_close()
{
	if (sys_init>0) {
		sys_init--;
		if (sys_init) return;
		last_update_time = 0xFFFFFFFF;
	}
}


#ifndef GetCurrentPermissions
DWORD GetCurrentPermissions();
#endif
#ifndef SetProcPermissions
void SetProcPermissions(DWORD );
#endif

/*CPU and Memory Usage*/
Bool gf_sys_get_rti(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
	THREADENTRY32 tentry;
	MEMORYSTATUS ms;
	u64 creation, exit, kernel, user, total_cpu_time, process_cpu_time;
	u32 entry_time;
	HANDLE hSnapShot;
	DWORD orig_perm;	

	if (!rti) return 0;

	memset(rti, 0, sizeof(GF_SystemRTInfo));
	entry_time = gf_sys_clock();
	if (last_update_time && (entry_time - last_update_time < refresh_time_ms)) {
		memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
		return 0;
	}

	if (flags & GF_RTI_SYSTEM_MEMORY_ONLY) {
		memset(rti, 0, sizeof(GF_SystemRTInfo));
		rti->sampling_instant = last_update_time;
		GlobalMemoryStatus(&ms);
		rti->physical_memory = ms.dwTotalPhys;
		rti->physical_memory_avail = ms.dwAvailPhys;
		return 1;
	}

	total_cpu_time = process_cpu_time = 0;

	/*get a snapshot of all running threads*/
	orig_perm = GetCurrentPermissions();
	SetProcPermissions(0xFFFFFFFF);
	hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0); 
	if (!hSnapShot) return 0;
	tentry.dwSize = sizeof(THREADENTRY32); 
	the_rti.thread_count = 0;
	/*note we always act as if GF_RTI_ALL_PROCESSES_TIMES flag is set, since there is no other way
	to enumerate threads from a process, and GetProcessTimes doesn't exist on CE*/
	if (Thread32First(hSnapShot, &tentry)) {
		do {
			/*get thread times*/
			if (GetThreadTimes( (HANDLE) tentry.th32ThreadID, (FILETIME *) &creation, (FILETIME *) &exit, (FILETIME *) &kernel, (FILETIME *) &user)) {
				total_cpu_time += user + kernel;
				if (tentry.th32OwnerProcessID==the_rti.pid) {
					process_cpu_time += user + kernel;
					the_rti.thread_count ++;
				}
			}
		} while (Thread32Next(hSnapShot, &tentry));
	}
	CloseToolhelp32Snapshot(hSnapShot); 

	if (flags & GF_RTI_PROCESS_MEMORY) {
		HEAPLIST32 hlentry;
		HEAPENTRY32 hentry;
		the_rti.process_memory = 0;
		hlentry.dwSize = sizeof(HEAPLIST32); 
		hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, the_rti.pid); 
		if (hSnapShot && Heap32ListFirst(hSnapShot, &hlentry)) {
			do {
				hentry.dwSize = sizeof(hentry);
				if (Heap32First(hSnapShot, &hentry, hlentry.th32ProcessID, hlentry.th32HeapID)) {
					do {
						the_rti.process_memory += hentry.dwBlockSize;
					} while (Heap32Next(hSnapShot, &hentry));
				}
			} while (Heap32ListNext(hSnapShot, &hlentry));
		}
		CloseToolhelp32Snapshot(hSnapShot); 
	}
	SetProcPermissions(orig_perm);
	total_cpu_time /= 10;
	process_cpu_time /= 10;

	the_rti.sampling_instant = last_update_time;
	if (last_update_time) {
		the_rti.sampling_period_duration = entry_time - last_update_time;
		the_rti.process_cpu_time_diff = (u32) ((process_cpu_time - last_process_k_u_time)/1000);

		the_rti.total_cpu_time_diff = (u32) ((total_cpu_time - last_total_k_u_time)/1000);
		/*we're not that accurate....*/
		if (the_rti.total_cpu_time_diff > the_rti.sampling_period_duration) 
			the_rti.sampling_period_duration = the_rti.total_cpu_time_diff;
	
		/*rough values*/
		the_rti.cpu_idle_time = the_rti.sampling_period_duration - the_rti.total_cpu_time_diff;
		the_rti.total_cpu_usage = (u32) (100 * the_rti.total_cpu_time_diff / the_rti.sampling_period_duration);
		the_rti.process_cpu_usage = (u32) (100*the_rti.process_cpu_time_diff / (the_rti.total_cpu_time_diff + the_rti.cpu_idle_time) );
	}
	last_total_k_u_time = total_cpu_time;
	last_process_k_u_time = process_cpu_time;
	last_update_time = entry_time;

	GlobalMemoryStatus(&ms);
	the_rti.physical_memory = ms.dwTotalPhys;
	the_rti.physical_memory_avail = ms.dwAvailPhys;
	if (!the_rti.process_memory) the_rti.process_memory = mem_usage_at_startup - ms.dwAvailPhys;
	memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
	return 1;
}
