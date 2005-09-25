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
#include <tlhelp32.h>


void gf_sleep(u32 ms)
{
	Sleep(ms);
}

static u32 clock_init = 0;

static void close_rti();

static LARGE_INTEGER frequency = {0, 0};
static LARGE_INTEGER init_counter = {0, 0};

static u32 (*W32_GetSysClock)();
static u64 (*W32_GetSysClockHigh)();

u64 W32_GetSysClockHIGHRES_NS()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	now.QuadPart -= init_counter.QuadPart;
	return now.QuadPart * 1000000 / frequency.QuadPart;
}

u32 W32_GetSysClockHIGHRES()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	now.QuadPart -= init_counter.QuadPart;
	return (u32) (now.QuadPart * 1000 / frequency.QuadPart);
}

u32 W32_GetSysClockNORMAL()
{
	return timeGetTime();
}

u64 W32_GetSysClockNORMAL_NS()
{
	return timeGetTime() * 1000;
}

void gf_sys_clock_start()
{
	if (!clock_init) {
		if (QueryPerformanceFrequency(&frequency)) {
			QueryPerformanceCounter(&init_counter);
			W32_GetSysClock = W32_GetSysClockHIGHRES;
			W32_GetSysClockHigh = W32_GetSysClockHIGHRES_NS;
		} else {
			timeBeginPeriod(1);
			W32_GetSysClock = W32_GetSysClockNORMAL;
			W32_GetSysClockHigh = W32_GetSysClockNORMAL_NS;
		}
	}
	clock_init += 1;
}

void gf_sys_clock_stop()
{
	if (clock_init > 0) {
		clock_init --;
		if (!clock_init) {
			if (!frequency.QuadPart) timeEndPeriod(1);
			close_rti();
		}
	}
}

u32 gf_sys_clock()
{
	return W32_GetSysClock();
}

u64 gf_sys_clock_ns()
{
	return W32_GetSysClockHigh();
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
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, gf_enum_dir_item enum_dir_fct, void *cbck)
{
	unsigned char path[GF_MAX_PATH], item_path[GF_MAX_PATH];
	WIN32_FIND_DATA FindData;
	HANDLE SearchH;

	if (!dir || !enum_dir_fct) return GF_BAD_PARAM;
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
		if (enum_dir_fct(cbck, FindData.cFileName, item_path)) {
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


u64 gf_f64_tell(FILE *fp)
{
	fpos_t pos;
	if (fgetpos(fp, &pos))
		return (u64) -1;
	else
		return ((u64) pos);
}

u64 gf_f64_seek(FILE *fp, s64 offset, s32 whence)
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

FILE *gf_f64_open(const char *file_name, const char *mode)
{
	return fopen(file_name, mode);
}



typedef BOOL(WINAPI* NTGetSystemTimes)(VOID *,VOID *,VOID *);
NTGetSystemTimes MyGetSystemTimes = NULL;

typedef BOOL(WINAPI* NTGetProcessMemoryInfo)(HANDLE,VOID *,DWORD);
NTGetProcessMemoryInfo MyGetProcessMemoryInfo = NULL;

typedef int(WINAPI* NTQuerySystemInfo)(ULONG,PVOID,ULONG,PULONG);
NTQuerySystemInfo MyQuerySystemInfo = NULL;

static u64 last_update_time = 0;
static u64 last_total_cpu_time = 0;
static u64 last_process_cpu_time = 0;
static u64 last_proc_idle_time = 0;
static u64 last_proc_k_u_time = 0;
static u32 pid = 0;

static Bool rti_init = 0;
static HINSTANCE psapi_hinst = NULL;


#ifndef PROCESS_MEMORY_COUNTERS
typedef struct _PROCESS_MEMORY_COUNTERS 
{  
	DWORD cb;
	DWORD PageFaultCount;
	SIZE_T PeakWorkingSetSize;
	SIZE_T WorkingSetSize;
	SIZE_T QuotaPeakPagedPoolUsage;
	SIZE_T QuotaPagedPoolUsage;
	SIZE_T QuotaPeakNonPagedPoolUsage;
	SIZE_T QuotaNonPagedPoolUsage;
	SIZE_T PagefileUsage;
	SIZE_T PeakPagefileUsage;
} PROCESS_MEMORY_COUNTERS;
#endif

#ifndef SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION
typedef struct _SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION 
{
	LARGE_INTEGER IdleTime;
	LARGE_INTEGER KernelTime;
	LARGE_INTEGER UserTime;
	LARGE_INTEGER Reserved1[2];
	ULONG Reserved2;
} SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION;
#endif

/*CPU and Memory Usage*/
Bool gf_get_sys_rt_info(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
	MEMORYSTATUS ms;
	u64 creation, exit, kernel, user, total_cpu_time, process_cpu_time, entry_time, proc_idle_time, proc_k_u_time;
	u32 process_mem_size, nb_threads;
	HANDLE hSnapShot;

	if (!rti) return 0;

	proc_idle_time = proc_k_u_time = total_cpu_time = process_cpu_time = 0;
	nb_threads = 0;
	process_mem_size = 0;

	memset(rti, 0, sizeof(GF_SystemRTInfo));
	rti->sampling_instant = last_update_time;

	if (!pid) pid = GetCurrentProcessId();

	if ((flags & GF_RTI_SYSTEM_MEMORY_ONLY) || !clock_init) goto default_vals;

	entry_time = gf_sys_clock_ns();
	if (last_update_time && (entry_time - last_update_time < refresh_time_ms*1000)) goto default_vals;
	
	
	if (!rti_init) {
		MyGetSystemTimes = (NTGetSystemTimes) GetProcAddress(GetModuleHandle("kernel32.dll"), "GetSystemTimes");
		MyQuerySystemInfo = (NTQuerySystemInfo) GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQuerySystemInformation");
		psapi_hinst = LoadLibrary("psapi.dll");
		MyGetProcessMemoryInfo = (NTGetProcessMemoryInfo) GetProcAddress(psapi_hinst, "GetProcessMemoryInfo");
		rti_init = 1;
	}
	/*XP-SP1 and Win2003 servers only have GetSystemTimes support. This will give a better estimation
	of CPU usage since we can take into account the idle time*/
	if (MyGetSystemTimes) {
		u64 u_time;
		MyGetSystemTimes(&proc_idle_time, &proc_k_u_time, &u_time);
		proc_k_u_time += u_time;
		proc_idle_time /= 10;
		proc_k_u_time /= 10;
	}
	/*same rq for NtQuerySystemInformation*/
	else if (MyQuerySystemInfo) {
		DWORD ret;
		SYSTEM_PROCESSOR_PERFORMANCE_INFORMATION info;
		MyQuerySystemInfo(0x8 /*SystemProcessorPerformanceInformation*/, &info, sizeof(info), &ret); 
		if (ret && (ret<=sizeof(info))) {
			proc_idle_time = info.IdleTime.QuadPart / 10;
			proc_k_u_time = (info.KernelTime.QuadPart + info.UserTime.QuadPart) / 10;
		}
	}
	/*no special API available, ONLY FETCH TIMES if requested (may eat up some time)*/
	else if (flags & GF_RTI_ALL_PROCESSES_TIMES) {
	    PROCESSENTRY32 pentry; 
		/*get a snapshot of all running threads*/
		hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); 
		if (!hSnapShot) return 0;
		pentry.dwSize = sizeof(PROCESSENTRY32); 
		if (Process32First(hSnapShot, &pentry)) {
			do {
				HANDLE procH = NULL;
				if (pentry.th32ProcessID) procH = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pentry.th32ProcessID);
				if (procH && GetProcessTimes(procH, (FILETIME *) &creation, (FILETIME *) &exit, (FILETIME *) &kernel, (FILETIME *) &user) ) {
					user += kernel;
					total_cpu_time += user;
					if (pentry.th32ProcessID==pid) {
						process_cpu_time = user;
						nb_threads = pentry.cntThreads;
					}
				}
				if (procH) CloseHandle(procH);
			} while (Process32Next(hSnapShot, &pentry));
		}
		CloseHandle(hSnapShot); 
		total_cpu_time /= 10;
	} 
	/*update total procs times if not done*/
	if (!total_cpu_time && proc_k_u_time) total_cpu_time = proc_k_u_time - proc_idle_time;

	if (!process_cpu_time) {
		HANDLE procH = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
		if (procH && GetProcessTimes(procH, (FILETIME *) &creation, (FILETIME *) &exit, (FILETIME *) &kernel, (FILETIME *) &user) ) {
			process_cpu_time = user + kernel;
		}
		if (procH) CloseHandle(procH);
	}
	process_cpu_time /= 10;

	/*this won't cost a lot*/
	if (MyGetProcessMemoryInfo) {
		PROCESS_MEMORY_COUNTERS pmc;
		HANDLE procH = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pid);
		MyGetProcessMemoryInfo(procH, &pmc, sizeof (pmc));
		process_mem_size = pmc.WorkingSetSize;
		if (procH) CloseHandle(procH);
	}
	else if (1) {
		u32 i;
		DWORD ret;
		HANDLE ph[2048];
		ret = GetProcessHeaps(2048, ph);
		if (ret<2048) {
			for (i=0; i<ret; i++) process_mem_size += HeapSize(ph[i], 0, NULL);
		}
	}

	/*THIS IS VERY HEAVY (eats up mem and time) - only perform if requested*/
	else if (flags & GF_RTI_PROCESS_MEMORY) {
		HEAPLIST32 hlentry;
		HEAPENTRY32 hentry;
		hlentry.dwSize = sizeof(HEAPLIST32); 
		hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, pid); 
		if (hSnapShot && Heap32ListFirst(hSnapShot, &hlentry)) {
			do {
				hentry.dwSize = sizeof(hentry);
				if (Heap32First(&hentry, hlentry.th32ProcessID, hlentry.th32HeapID)) {
					do {
						process_mem_size += hentry.dwBlockSize;
					} while (Heap32Next(&hentry));
				}
			} while (Heap32ListNext(hSnapShot, &hlentry));
		}
		CloseHandle(hSnapShot); 
	}

	if (last_update_time) {
		rti->sampling_period_duration = entry_time - last_update_time;

		rti->total_cpu_time_diff = (total_cpu_time - last_total_cpu_time);
		/*we're not that accurate....*/
		if (rti->total_cpu_time_diff > rti->sampling_period_duration) 
			rti->sampling_period_duration = rti->total_cpu_time_diff;
		
		rti->process_cpu_time_diff = process_cpu_time - last_process_cpu_time;
		if (proc_k_u_time) {
			u64 samp_sys_time = proc_k_u_time - last_proc_k_u_time;
			rti->cpu_idle_time = proc_idle_time - last_proc_idle_time;
            rti->cpu_usage = (u32) ( (samp_sys_time - rti->cpu_idle_time) / (samp_sys_time / 100) );
		} else {
			/*rough values*/
			rti->cpu_idle_time = rti->sampling_period_duration - rti->total_cpu_time_diff;
			rti->cpu_usage = (u32) (rti->total_cpu_time_diff * 100 / rti->sampling_period_duration);
		}
	}

	last_total_cpu_time = total_cpu_time;
	last_process_cpu_time = process_cpu_time;
	last_update_time = entry_time;
	last_proc_idle_time = proc_idle_time;
	last_proc_k_u_time = proc_k_u_time;

default_vals:
	rti->total_cpu_time = last_total_cpu_time;
	rti->process_cpu_time = last_process_cpu_time;
	rti->process_memory = process_mem_size;
	rti->thread_count = nb_threads;
	rti->pid = pid;
	GlobalMemoryStatus(&ms);
	rti->physical_memory = ms.dwTotalPhys;
	rti->physical_memory_avail = ms.dwAvailPhys;

	return 1;
}

static void close_rti()
{
	MyGetSystemTimes = NULL;
	MyGetProcessMemoryInfo = NULL;
	MyQuerySystemInfo = NULL;
	last_update_time = last_total_cpu_time = last_process_cpu_time = last_proc_idle_time = last_proc_k_u_time = 0;
	pid = 0;
	if (psapi_hinst) {
		FreeLibrary(psapi_hinst);
		psapi_hinst = NULL;
	}
	rti_init = 0;
}