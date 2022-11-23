/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2022
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
#include <gpac/network.h>
#include <gpac/config_file.h>

#if defined(_WIN32_WCE)

#include <winbase.h>
#include <winsock.h>
#include <tlhelp32.h>
//#include <direct.h>

#if !defined(__GNUC__)
#pragma comment(lib, "toolhelp")
#endif

#elif defined(WIN32)

#include <time.h>
#include <sys/timeb.h>
#include <io.h>
#include <windows.h>
#include <tlhelp32.h>
#include <direct.h>

#if !defined(__GNUC__)
#pragma comment(lib, "winmm")
#endif

#else

#include <time.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <dirent.h>
#include <unistd.h>
#include <sys/times.h>
#include <sys/resource.h>

#ifndef __BEOS__
#include <errno.h>
#endif

#define SLEEP_ABS_SELECT		1

static u32 sys_start_time = 0;
static u64 sys_start_time_hr = 0;
#endif


#ifndef _WIN32_WCE
#include <locale.h>
#endif


#include <gpac/revision.h>
#define GPAC_FULL_VERSION       GPAC_VERSION "-rev" GPAC_GIT_REVISION

#define GPAC_COPYRIGHT "(c) 2000-2022 Telecom Paris distributed under LGPL v2.1+ - http://gpac.io"

GF_EXPORT
const char *gf_gpac_version()
{
	return GPAC_FULL_VERSION;
}

#ifdef GPAC_MP4BOX_MINI
#define MINI_BUILD_DISCLAIMER "\n\tMINI build (encoders, decoders, audio and video output disabled)"
#endif

GF_EXPORT
const char *gf_gpac_copyright()
{
	return GPAC_COPYRIGHT
#ifdef GPAC_MP4BOX_MINI
		MINI_BUILD_DISCLAIMER
#endif
		;
}
GF_EXPORT
const char *gf_gpac_copyright_cite()
{
	return GPAC_COPYRIGHT
#ifdef GPAC_MP4BOX_MINI
		MINI_BUILD_DISCLAIMER
#endif
		"\n\n" \
			"Please cite our work in your research:\n"
		"\tGPAC Filters: https://doi.org/10.1145/3339825.3394929\n"
		"\tGPAC: https://doi.org/10.1145/1291233.1291452\n"
		;

}

GF_EXPORT
u32 gf_gpac_abi_major()
{
	return GPAC_VERSION_MAJOR;
}
GF_EXPORT
u32 gf_gpac_abi_minor()
{
	return GPAC_VERSION_MINOR;
}
GF_EXPORT
u32 gf_gpac_abi_micro()
{
	return GPAC_VERSION_MICRO;
}


#ifndef WIN32

GF_EXPORT
u32 gf_sys_clock()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return (u32) ( ( (now.tv_sec)*1000 + (now.tv_usec) / 1000) - sys_start_time );
}

GF_EXPORT
u64 gf_sys_clock_high_res()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return (now.tv_sec)*1000000 + (now.tv_usec) - sys_start_time_hr;
}

#endif

static Bool gf_sys_enable_remotery(Bool start, Bool is_shutdown);


GF_EXPORT
void gf_sleep(u32 ms)
{
	gf_rmt_begin(sleep, GF_RMT_AGGREGATE);

#ifdef WIN32
	Sleep(ms);
#else
	s32 sel_err;
	struct timeval tv;

#ifndef SLEEP_ABS_SELECT
	u32 prev, now, elapsed;
#endif

#ifdef SLEEP_ABS_SELECT
	tv.tv_sec = ms/1000;
	tv.tv_usec = (ms%1000)*1000;
#else
	prev = gf_sys_clock();
#endif

	do {
		errno = 0;

#ifndef SLEEP_ABS_SELECT
		now = gf_sys_clock();
		elapsed = (now - prev);
		if ( elapsed >= ms ) {
			break;
		}
		prev = now;
		ms -= elapsed;
		tv.tv_sec = ms/1000;
		tv.tv_usec = (ms%1000)*1000;
#endif

		sel_err = select(0, NULL, NULL, NULL, &tv);
	} while ( sel_err && (errno == EINTR) );
#endif

	gf_rmt_end();

}

#ifndef gettimeofday
#ifdef _WIN32_WCE

#include <time.h>
//#include <wce_time.h>

/*
 * Author of first version (timeval.h): by Wu Yongwei
 * Author of Windows CE version: Mateusz Loskot (mateusz@loskot.net)
 *
 * All code here is considered in the public domain though we do wish our names
 * could be retained if anyone uses them.
 */

/*
 * Constants used internally by time functions.
 */

#ifndef _TM_DEFINED
struct tm
{
	int tm_sec;     /* seconds after the minute - [0,59] */
	int tm_min;     /* minutes after the hour - [0,59] */
	int tm_hour;	/* hours since midnight - [0,23] */
	int tm_mday;	/* day of the month - [1,31] */
	int tm_mon;     /* months since January - [0,11] */
	int tm_year;	/* years since 1900 */
	int tm_wday;	/* days since Sunday - [0,6] */
	int tm_yday;	/* days since January 1 - [0,365] */
	int tm_isdst;	/* daylight savings time flag */
};
#define _TM_DEFINED
#endif /* _TM_DEFINED */

#ifndef _TIMEZONE_DEFINED
struct timezone
{
	int tz_minuteswest; /* minutes W of Greenwich */
	int tz_dsttime;     /* type of dst correction */
};
#define _TIMEZONE_DEFINED
#endif /* _TIMEZONE_DEFINED */


#if defined(_MSC_VER) || defined(__BORLANDC__)
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif

int gettimeofday(struct timeval *tp, struct timezone *tzp)
{
	SYSTEMTIME      st;
	FILETIME        ft;
	LARGE_INTEGER   li;
	TIME_ZONE_INFORMATION tzi;
	__int64         t;

	if (NULL != tp)
	{
		GetSystemTime(&st);
		SystemTimeToFileTime(&st, &ft);
		li.LowPart  = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		t  = li.QuadPart;       /* In 100-nanosecond intervals */
		t -= EPOCHFILETIME;     /* Offset to the Epoch time */
		t /= 10;                /* In microseconds */
		tp->tv_sec  = (long)(t / 1000000);
		tp->tv_usec = (long)(t % 1000000);
	}

	if (NULL != tzp)
	{
		GetTimeZoneInformation(&tzi);

		tzp->tz_minuteswest = tzi.Bias;
		if (tzi.StandardDate.wMonth != 0)
		{
			tzp->tz_minuteswest += tzi.StandardBias * 60;
		}

		if (tzi.DaylightDate.wMonth != 0)
		{
			tzp->tz_dsttime = 1;
		}
		else
		{
			tzp->tz_dsttime = 0;
		}
	}

	return 0;
}


#if _GPAC_UNUSED
/*
	time between jan 1, 1601 and jan 1, 1970 in units of 100 nanoseconds
	FILETIME in Win32 is from jan 1, 1601
*/

s32 __gettimeofday(struct timeval *tp, void *tz)
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


#elif defined(WIN32)

static s32 gettimeofday(struct timeval *tp, void *tz)
{
	struct _timeb timebuffer;

	_ftime( &timebuffer );
	tp->tv_sec  = (long) (timebuffer.time);
	tp->tv_usec = timebuffer.millitm * 1000;
	return 0;
}
#endif

#endif

#ifdef _WIN32_WCE

void CE_Assert(u32 valid, char *file, u32 line)
{
	if (!valid) {
		char szBuf[2048];
		u16 wcBuf[2048];
		sprintf(szBuf, "File %s : line %d", file, line);
		CE_CharToWide(szBuf, wcBuf);
		MessageBox(NULL, wcBuf, _T("GPAC Assertion Failure"), MB_OK);
		exit(EXIT_FAILURE);
	}
}

void CE_WideToChar(unsigned short *w_str, char *str)
{
	WideCharToMultiByte(CP_ACP, 0, w_str, -1, str, GF_MAX_PATH, NULL, NULL);
}

void CE_CharToWide(char *str, unsigned short *w_str)
{
	MultiByteToWideChar(CP_ACP, 0, str, -1, w_str, GF_MAX_PATH);
}


#endif

GF_EXPORT
void gf_rand_init(Bool Reset)
{
	if (Reset) {
		srand(1);
	} else {
#if defined(_WIN32_WCE)
		srand( (u32) GetTickCount() );
#else
		srand( (u32) time(NULL) );
#endif
	}
}

GF_EXPORT
u32 gf_rand()
{
	return rand();
}

#ifndef _WIN32_WCE
#include <sys/stat.h>
#endif

GF_EXPORT
void gf_utc_time_since_1970(u32 *sec, u32 *msec)
{
#if defined (WIN32) && !defined(_WIN32_WCE)
	struct _timeb	tb;
	_ftime( &tb );
	*sec = (u32) tb.time;
	*msec = tb.millitm;
#else
	struct timeval tv;
	gettimeofday(&tv, NULL);
	*sec = (u32) tv.tv_sec;
	*msec = tv.tv_usec/1000;
#endif
}

GF_EXPORT
void gf_get_user_name(char buf[1024])
{
	strcpy(buf, "gpac-user");

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
#if 0
	struct passwd *pw;
	pw = getpwuid(getuid());
	strcpy(buf, "");
	if (pw && pw->pw_name) {
		strncpy(name, pw->pw_name, 1023);
		name[1023] = 0;
	}
#endif
}


#ifndef WIN32
GF_EXPORT
char * my_str_upr(char *str)
{
	u32 i;
	for (i=0; i<strlen(str); i++) {
		str[i] = toupper(str[i]);
	}
	return str;
}

GF_EXPORT
char * my_str_lwr(char *str)
{
	u32 i;
	for (i=0; i<strlen(str); i++) {
		str[i] = tolower(str[i]);
	}
	return str;
}
#endif

/*seems OK under mingw also*/
#ifdef WIN32
#ifdef _WIN32_WCE

Bool gf_prompt_has_input()
{
	return 0;
}
char gf_prompt_get_char() {
	return 0;
}
GF_EXPORT
void gf_prompt_set_echo_off(Bool echo_off) {
	return;
}
GF_Err gf_prompt_get_size(u32 *width, u32 *height)
{
	return GF_NOT_SUPPORTED;
}

#else

#include <conio.h>
#include <windows.h>

Bool gf_prompt_has_input()
{
	return kbhit();
}

char gf_prompt_get_char()
{
	return getchar();
}

GF_EXPORT
void gf_prompt_set_echo_off(Bool echo_off)
{
	DWORD flags;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	BOOL ret = GetConsoleMode(hStdin, &flags);
	if (!ret) {
		DWORD err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONSOLE, ("[Console] GetConsoleMode() return with the following error code: %d\n", err));
		return;
	}
	if (echo_off) flags &= ~ENABLE_ECHO_INPUT;
	else flags |= ENABLE_ECHO_INPUT;
	SetConsoleMode(hStdin, flags);
}

GF_EXPORT
GF_Err gf_prompt_get_size(u32 *width, u32 *height)
{
    CONSOLE_SCREEN_BUFFER_INFO info;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	BOOL ret = GetConsoleScreenBufferInfo(hStdin, &info);

	if (!ret) {
		DWORD err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONSOLE, ("[Console] GetConsoleScreenBufferInfo() return with the following error code: %d\n", err));
		return GF_IO_ERR;
	}
	if (width) *width = info.dwSize.X;
	if (height) *height = info.dwSize.Y;
	return GF_OK;
}

#endif
#else
/*linux kbhit/getchar- borrowed on debian mailing lists, (author Mike Brownlow)*/
#include <termios.h>
#include <sys/ioctl.h>

static struct termios t_orig, t_new;
static s32 ch_peek = -1;

static void init_keyboard()
{
	tcgetattr(0, &t_orig);
	t_new = t_orig;
	t_new.c_lflag &= ~ICANON;
	t_new.c_lflag &= ~ECHO;
	t_new.c_lflag &= ~ISIG;
	t_new.c_cc[VMIN] = 1;
	t_new.c_cc[VTIME] = 0;
	tcsetattr(0, TCSANOW, &t_new);
}
static void close_keyboard(Bool new_line)
{
	tcsetattr(0,TCSANOW, &t_orig);
	if (new_line) fprintf(stderr, "\n");
}

GF_EXPORT
void gf_prompt_set_echo_off(Bool echo_off)
{
	init_keyboard();
	if (echo_off) t_orig.c_lflag &= ~ECHO;
	else t_orig.c_lflag |= ECHO;
	close_keyboard(GF_FALSE);
}

GF_EXPORT
Bool gf_prompt_has_input()
{
	u8 ch;
	s32 nread;
	pid_t fg = tcgetpgrp(STDIN_FILENO);

	//we are not foreground nor piped (used for IDEs), can't read stdin
	if ((fg!=-1) && (fg != getpgrp())) {
		return GF_FALSE;
	}
	init_keyboard();
	if (ch_peek != -1) return GF_TRUE;
	t_new.c_cc[VMIN]=0;
	tcsetattr(0, TCSANOW, &t_new);
	nread = (s32) read(0, &ch, 1);
	t_new.c_cc[VMIN]=1;
	tcsetattr(0, TCSANOW, &t_new);
	if(nread == 1) {
		ch_peek = ch;
		return GF_TRUE;
	}
	close_keyboard(GF_FALSE);
	return GF_FALSE;
}

GF_EXPORT
char gf_prompt_get_char()
{
	char ch;
	if (ch_peek != -1) {
		ch = ch_peek;
		ch_peek = -1;
		close_keyboard(GF_TRUE);
		return ch;
	}
	if (0==read(0,&ch,1))
		ch = 0;
	close_keyboard(GF_TRUE);
	return ch;
}

GF_EXPORT
GF_Err gf_prompt_get_size(u32 *width, u32 *height)
{
#if defined(TIOCGWINSZ)
    struct winsize ws;
    if (ioctl(STDIN_FILENO, TIOCGWINSZ, &ws) != 0) return GF_IO_ERR;

    if (width) *width = ws.ws_col;
    if (height) *height = ws.ws_row;
    return GF_OK;
#elif defined(WIOCGETD)
	struct uwdata w;
	if (ioctl(2, WIOCGETD, &w) != 0) return GF_IO_ERR;

	if (width && (w.uw_width > 0))
		*width = w.uw_width / w.uw_hs;
	if (height && (w.uw_height > 0))
		*height = w.uw_height / w.uw_vs;
    return GF_OK;
#else
    return GF_NOT_SUPPORTED;
#endif
}

#endif


static u32 sys_init = 0;
static u32 last_update_time = 0;
static u64 last_process_k_u_time = 0;
GF_SystemRTInfo the_rti;


#if defined(_WIN32_WCE)
static LARGE_INTEGER frequency , init_counter;
static u64 last_total_k_u_time = 0;
static u32 mem_usage_at_startup = 0;


#ifndef GetCurrentPermissions
DWORD GetCurrentPermissions();
#endif
#ifndef SetProcPermissions
void SetProcPermissions(DWORD );
#endif

#elif defined(WIN32)
static LARGE_INTEGER frequency , init_counter;
static u64 last_proc_idle_time = 0;
static u64 last_proc_k_u_time = 0;

static HINSTANCE psapi_hinst = NULL;
typedef BOOL(WINAPI* NTGetSystemTimes)(VOID *,VOID *,VOID *);
NTGetSystemTimes MyGetSystemTimes = NULL;
typedef BOOL(WINAPI* NTGetProcessMemoryInfo)(HANDLE,VOID *,DWORD);
NTGetProcessMemoryInfo MyGetProcessMemoryInfo = NULL;
typedef int(WINAPI* NTQuerySystemInfo)(ULONG,PVOID,ULONG,PULONG);
NTQuerySystemInfo MyQuerySystemInfo = NULL;

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


#else

static u64 last_cpu_u_k_time = 0;
static u64 last_cpu_idle_time = 0;
static u64 mem_at_startup = 0;

#endif

#ifdef WIN32
static u32 (*OS_GetSysClock)();

u32 gf_sys_clock()
{
	return OS_GetSysClock();
}


static u64 (*OS_GetSysClockHR)();
u64 gf_sys_clock_high_res()
{
	return OS_GetSysClockHR();
}
#endif


#ifdef WIN32

static u32 OS_GetSysClockHIGHRES()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	now.QuadPart -= init_counter.QuadPart;
	return (u32) ((now.QuadPart * 1000) / frequency.QuadPart);
}

static u64 OS_GetSysClockHIGHRES_FULL()
{
	LARGE_INTEGER now;
	QueryPerformanceCounter(&now);
	now.QuadPart -= init_counter.QuadPart;
	return (u64) ((now.QuadPart * 1000000) / frequency.QuadPart);
}

static u32 OS_GetSysClockNORMAL()
{
#ifdef _WIN32_WCE
	return GetTickCount();
#else
	return timeGetTime();
#endif
}

static u64 OS_GetSysClockNORMAL_FULL()
{
	u64 res = OS_GetSysClockNORMAL();
	return res*1000;
}

#endif /* WIN32 */

#if defined(__sh__)
/* Avoid exception for denormalized floating point values */
static int
sh4_get_fpscr()
{
	int ret;
	asm volatile ("sts fpscr,%0" : "=r" (ret));
	return ret;
}

static void
sh4_put_fpscr(int nv)
{
	asm volatile ("lds %0,fpscr" : : "r" (nv));
}

#define SH4_FPSCR_FR 0x00200000
#define SH4_FPSCR_SZ 0x00100000
#define SH4_FPSCR_PR 0x00080000
#define SH4_FPSCR_DN 0x00040000
#define SH4_FPSCR_RN 0x00000003
#define SH4_FPSCR_RN_N 0
#define SH4_FPSCR_RN_Z 1

extern int __fpscr_values[2];

void
sh4_change_fpscr(int off, int on)
{
	int b = sh4_get_fpscr();
	off = ~off;
	off |=   0x00180000;
	on  &= ~ 0x00180000;
	b &= off;
	b |= on;
	sh4_put_fpscr(b);
	__fpscr_values[0] &= off;
	__fpscr_values[0] |= on;
	__fpscr_values[1] &= off;
	__fpscr_values[1] |= on;
}

#endif

GF_EXPORT
struct tm *gf_gmtime(const time_t *time)
{
#ifdef _WIN32_WCE
	FILETIME filet;
	LPSYSTEMTIME syst;
	*(LONGLONG *) &filet = (sec/* - GF_NTP_SEC_1900_TO_1970*/) * 10000000 + TIMESPEC_TO_FILETIME_OFFSET;
	FileTimeToSystemTime(&filet, &syst);
	if (syst.wSecond>60)
		syst.wSecond=60;
#endif

	struct tm *tm = gmtime(time);
	//see issue #859, no clue how this happened...
	if (tm->tm_sec>60)
		tm->tm_sec = 60;
	return tm;
}


#ifdef GPAC_MEMORY_TRACKING
void gf_mem_enable_tracker(Bool enable_backtrace);
#endif

static u64 memory_at_gpac_startup = 0;

static u32 gpac_argc = 0;
const char **gpac_argv = NULL;
Bool *gpac_argv_state = NULL;
static Bool gpac_test_mode = GF_FALSE;
static Bool gpac_old_arch = GF_FALSE;
static Bool gpac_discard_config = GF_FALSE;

//in error.c
#ifndef GPAC_DISABLE_LOG
extern FILE *gpac_log_file;
extern Bool gpac_log_time_start;
extern Bool gpac_log_utc_time;
#endif

GF_EXPORT
Bool gf_log_use_file()
{
#ifndef GPAC_DISABLE_LOG
	return gpac_log_file ? GF_TRUE : GF_FALSE;
#else
	return GF_FALSE;
#endif
}

static void progress_quiet(const void *cbck, const char *title, u64 done, u64 total) { }

void gpac_disable_progress()
{
	gf_set_progress_callback(NULL, progress_quiet);
}

GF_EXPORT
Bool gf_sys_is_test_mode()
{
	return gpac_test_mode;
}

GF_EXPORT
Bool gf_sys_old_arch_compat()
{
	return gpac_old_arch;
}

#ifdef GPAC_ENABLE_COVERAGE
GF_EXPORT
Bool gf_sys_is_cov_mode()
{
	return gpac_test_mode;
}
#endif

const char *gpac_log_file_name=NULL;

GF_EXPORT
void gf_log_reset_file()
{
#ifndef GPAC_DISABLE_LOG
	if (gpac_log_file_name) {
		if (gpac_log_file) gf_fclose(gpac_log_file);
		gpac_log_file = gf_fopen(gpac_log_file_name, "wt");
	}
#endif
}

static Bool gpac_has_global_filter_args=GF_FALSE;
static Bool gpac_has_global_filter_meta_args=GF_FALSE;
#include <gpac/thread.h>
GF_Mutex *logs_mx = NULL;

Bool gf_sys_has_filter_global_args()
{
	return gpac_has_global_filter_args;
}
Bool gf_sys_has_filter_global_meta_args()
{
	return gpac_has_global_filter_meta_args;
}

static u32 gpac_quiet = 0;
char gf_prog_lf = '\r';
extern Bool gpac_use_poll;

GF_EXPORT
GF_Err gf_sys_set_args(s32 argc, const char **argv)
{
	s32 i;
	if (!gpac_argc) {
		Bool gf_opts_load_option(const char *arg_name, const char *val, Bool *consumed_next, GF_Err *e);
		void gf_cfg_load_restrict();

		gpac_use_poll = GF_TRUE;

		for (i=1; i<argc; i++) {
			Bool consumed;
			GF_Err e;
			Bool use_sep = GF_FALSE;
			Bool bool_value = GF_TRUE;
			char szArg[1024];
			char *arg_val;
			const char *arg = argv[i];
			if (!arg) continue;

			arg_val = strchr(arg, '=');
			if (arg_val) {
				u32 len = (u32) (arg_val-arg);
				if (len>1023) len = 1023;
				strncpy(szArg, arg, len);
				szArg[len]=0;
				arg_val++;
				arg = szArg;
				use_sep = GF_TRUE;
			} else if (i+1<argc) {
				arg_val = (char *) argv[i+1];
			}
			if ((arg[0] != '-') || ! arg[1]) {
				continue;
			}
			if (arg_val && (!strcmp(arg_val, "no") || !strcmp(arg_val, "false") || !strcmp(arg_val, "°0") ) )
				bool_value = GF_FALSE;


			if (arg[1]=='-') {
				gpac_has_global_filter_args = GF_TRUE;
			} else if (arg[1]=='+') {
				gpac_has_global_filter_meta_args = GF_TRUE;
			} else if (!strcmp(arg, "-log-file") || !strcmp(arg, "-lf")) {
#ifndef GPAC_DISABLE_LOG
				gpac_log_file_name = arg_val;
#endif
				if (!use_sep) i += 1;
			} else if (!strcmp(arg, "-logs") ) {
				e = gf_log_set_tools_levels(arg_val, GF_FALSE);
				if (e) return e;

				if (!use_sep) i += 1;
			} else if (!strcmp(arg, "-log-clock") || !strcmp(arg, "-lc")) {
#ifndef GPAC_DISABLE_LOG
				gpac_log_time_start = GF_TRUE;
#endif
			} else if (!strcmp(arg, "-log-utc") || !strcmp(arg, "-lu")) {
#ifndef GPAC_DISABLE_LOG
				gpac_log_utc_time = GF_TRUE;
#endif
			} else if (!strcmp(arg, "-quiet")) {
				gpac_quiet = 2;
			} else if (!strcmp(arg, "-noprog")) {
				if (!gpac_quiet) gpac_quiet = 1;
			} else if (!strcmp(arg, "-proglf")) {
				gf_prog_lf = '\n';
			} else if (!stricmp(arg, "-for-test")) {
				gpac_test_mode = bool_value;
			} else if (!stricmp(arg, "-old-arch")) {
				gpac_old_arch = bool_value;
			} else if (!stricmp(arg, "-no-save")) {
				gpac_discard_config = bool_value;
			} else if (!stricmp(arg, "-no-poll")) {
				gpac_use_poll = bool_value;
			} else if (!stricmp(arg, "-ntp-shift")) {
				s32 shift = arg_val ? atoi(arg_val) : 0;
				gf_net_set_ntp_shift(shift);
				if (!use_sep) i += 1;
			} else if (gf_opts_load_option(arg, arg_val, &consumed, &e)) {
				if (e) return e;

				if (consumed && !use_sep)
					i += 1;
			}
		}

#ifdef GPAC_CONFIG_DARWIN
		//if running in xcode disable color logs (not supported by output console)
		if (getenv("__XCODE_BUILT_PRODUCTS_DIR_PATHS") != NULL) {
			gf_log_set_tools_levels("ncl", GF_FALSE);
		}
#endif


#ifndef GPAC_DISABLE_LOG
		if (gpac_log_file_name) {
			gpac_log_file = gf_fopen(gpac_log_file_name, "wt");
		}
#endif
		if (gf_opts_get_bool("core", "rmt"))
			gf_sys_enable_remotery(GF_TRUE, GF_FALSE);

		if (gpac_quiet) {
			if (gpac_quiet==2) gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_QUIET);
			gf_set_progress_callback(NULL, progress_quiet);
		}
		//now that we have parsed all options, load restrict
		gf_cfg_load_restrict();
	}
	//for OSX we allow overwrite of argc/argv due to different behavior between console-mode apps and GUI
#if !defined(__DARWIN__) && !defined(__APPLE__)
	if (!gpac_argc && (argc>=0) )
#endif
	{
		gpac_argc = (u32) argc;
		gpac_argv = argv;
		gpac_argv_state = gf_realloc(gpac_argv_state, sizeof(Bool) * argc);
		for (i=0; i<argc; i++)
			gpac_argv_state[i] = GF_FALSE;
	}
	return GF_OK;
}

GF_EXPORT
void gf_sys_mark_arg_used(s32 arg_idx, Bool used)
{
	if (arg_idx < (s32) gpac_argc)
		gpac_argv_state[arg_idx] = used;
}

GF_EXPORT
Bool gf_sys_is_arg_used(s32 arg_idx)
{
	if (arg_idx < (s32) gpac_argc)
		return  gpac_argv_state[arg_idx];
	return GF_FALSE;
}

GF_EXPORT
u32 gf_sys_is_quiet()
{
	return gpac_quiet;
}


GF_EXPORT
u32 gf_sys_get_argc()
{
	return gpac_argc;
}
GF_EXPORT
const char **gf_sys_get_argv()
{
	return gpac_argv;
}
GF_EXPORT
const char *gf_sys_get_arg(u32 arg)
{
	if (!gpac_argc || !gpac_argv) return NULL;
	if (arg>=gpac_argc) return NULL;
	return gpac_argv[arg];
}
GF_EXPORT
const char *gf_sys_find_global_arg(const char *arg)
{
	u32 i;
	if (!gpac_argc || !gpac_argv) return NULL;
	for (i=0; i<gpac_argc; i++) {
		const char *sep;
		u32 len;
		const char *an_arg = gpac_argv[i];
		if (an_arg[0]!='-') continue;
		if ((an_arg[1]!='-') && (an_arg[1]!='+')) continue;
		an_arg += 2;
		sep = strchr(an_arg, '@');
		if (sep) an_arg = sep+1;
		sep = strchr(an_arg, '=');
		if (sep) len = (u32) (sep - an_arg);
		else len = (u32) strlen(an_arg);
		if (len != (u32) strlen(arg)) continue;

		if (strncmp(an_arg, arg, len)) continue;

		if (!sep) return "";
		return sep;
	}
	return NULL;
}


#ifndef GPAC_DISABLE_REMOTERY
Remotery *remotery_handle=NULL;

//commented out as it put quite some load on the browser

gf_log_cbk gpac_prev_default_logs = NULL;

const char *gf_log_tool_name(GF_LOG_Tool log_tool);
const char *gf_log_level_name(GF_LOG_Level log_level);

void gpac_rmt_log_callback(void *cbck, GF_LOG_Level level, GF_LOG_Tool tool, const char *fmt, va_list vlist)
{
#ifndef GPAC_DISABLE_LOG

#define RMT_LOG_SIZE	5000
	char szMsg[RMT_LOG_SIZE];
	u32 len;
	sprintf(szMsg, "{ \"type\": \"logs\", \"level\": \"%s\" \"tool\": \"%s\", \"value\": \"", gf_log_level_name(level), gf_log_tool_name(tool));

	len = (u32) strlen(szMsg);
	vsnprintf(szMsg, RMT_LOG_SIZE - len - 3, fmt, vlist);
	strcat(szMsg, "\"}");

	rmt_LogText(szMsg);

#undef RMT_LOG_SIZE

#endif

}

static void *rmt_udta = NULL;
gf_rmt_user_callback rmt_usr_cbk = NULL;

static void gpac_rmt_input_handler(const char* text, void* context)
{
	if (text && rmt_usr_cbk)
		rmt_usr_cbk(rmt_udta, text);
}
#endif

static Bool gf_sys_enable_remotery(Bool start, Bool is_shutdown)
{
#ifndef GPAC_DISABLE_REMOTERY
	if (start && !remotery_handle) {
		rmtSettings *rmcfg = rmt_Settings();

		rmcfg->port = gf_opts_get_int("core", "rmt-port");
		rmcfg->reuse_open_port = gf_opts_get_bool("core", "rmt-reuse");
		rmcfg->limit_connections_to_localhost = gf_opts_get_bool("core", "rmt-localhost");
		rmcfg->msSleepBetweenServerUpdates = gf_opts_get_int("core", "rmt-sleep");
		rmcfg->maxNbMessagesPerUpdate = gf_opts_get_int("core", "rmt-nmsg");
		rmcfg->messageQueueSizeInBytes = gf_opts_get_int("core", "rmt-qsize");
		rmcfg->input_handler = gpac_rmt_input_handler;

		rmtError rme = rmt_CreateGlobalInstance(&remotery_handle);
		if (rme != RMT_ERROR_NONE) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] unable to initialize Remotery profiler: error %d\n", rme));
			return GF_FALSE;
		}
		//OpenGL binding is done upon loading of the driver, otherwise crashes on windows

		if (gf_opts_get_bool("core", "rmt-log")) {
			gpac_prev_default_logs = gf_log_set_callback(NULL, gpac_rmt_log_callback);
		}
#ifdef GPAC_ENABLE_COVERAGE
		if (gf_sys_is_cov_mode()) {
			gpac_rmt_input_handler(NULL, NULL);
		}
#endif

	} else if (!start && remotery_handle) {
		if (gf_opts_get_bool("core", "rmt-ogl"))
			rmt_UnbindOpenGL();

		rmt_DestroyGlobalInstance(remotery_handle);

		remotery_handle=NULL;
		if (gpac_prev_default_logs != NULL)
			gf_log_set_callback(NULL, gpac_prev_default_logs);
	}
	return GF_TRUE;
#else
	return GF_NOT_SUPPORTED;
#endif
}

GF_EXPORT
GF_Err gf_sys_profiler_set_callback(void *udta, gf_rmt_user_callback usr_cbk)
{
#ifndef GPAC_DISABLE_REMOTERY
	if (remotery_handle) {
		rmt_udta = udta;
		rmt_usr_cbk = usr_cbk;
		return GF_OK;
	}
	return GF_BAD_PARAM;
#else
	return GF_NOT_SUPPORTED;
#endif
}

GF_EXPORT
GF_Err gf_sys_profiler_send(const char *msg)
{
#ifndef GPAC_DISABLE_REMOTERY
	if (remotery_handle) {
		rmt_LogText(msg);
		return GF_OK;
	}
	return GF_BAD_PARAM;
#else
	return GF_NOT_SUPPORTED;
#endif
}

GF_EXPORT
void gf_sys_profiler_enable_sampling(Bool enable)
{
#ifndef GPAC_DISABLE_REMOTERY
	if (remotery_handle) {
		rmt_EnableSampling(enable);
	}
#endif
}

GF_EXPORT
Bool gf_sys_profiler_sampling_enabled()
{
#ifndef GPAC_DISABLE_REMOTERY
	if (remotery_handle) {
		return rmt_SamplingEnabled();
	}
#endif
	return GF_FALSE;
}

#include <gpac/list.h>
GF_List *all_blobs = NULL;

GF_EXPORT
GF_Err gf_blob_get(const char *blob_url, u8 **out_data, u32 *out_size, u32 *out_flags)
{
	GF_Blob *blob = NULL;
	if (strncmp(blob_url, "gmem://", 7)) return GF_BAD_PARAM;
	if (sscanf(blob_url, "gmem://%p", &blob) != 1) return GF_BAD_PARAM;
	if (!blob)
		return GF_BAD_PARAM;
	if (gf_list_find(all_blobs, blob)<0)
		return GF_URL_REMOVED;
	if (blob->data && blob->mx)
		gf_mx_p(blob->mx);
	if (out_data) *out_data = blob->data;
	if (out_size) *out_size = blob->size;
	if (out_flags) *out_flags = blob->flags;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_blob_release(const char *blob_url)
{
    GF_Blob *blob = NULL;
    if (strncmp(blob_url, "gmem://", 7)) return GF_BAD_PARAM;
    if (sscanf(blob_url, "gmem://%p", &blob) != 1) return GF_BAD_PARAM;
    if (!blob)
		return GF_BAD_PARAM;
	if (gf_list_find(all_blobs, blob)<0)
		return GF_URL_REMOVED;
    if (blob->data && blob->mx)
        gf_mx_v(blob->mx);
    return GF_OK;
}

GF_EXPORT
char *gf_blob_register(GF_Blob *blob)
{
	char szPath[200];
	if (!blob) return NULL;
	sprintf(szPath, "gmem://%p", blob);
	if (!all_blobs) {
		all_blobs = gf_list_new();
	} else {
		gf_list_del_item(all_blobs, blob);
	}
	gf_list_add(all_blobs, blob);

	return gf_strdup(szPath);
}

GF_EXPORT
void gf_blob_unregister(GF_Blob *blob)
{
	if (!blob) return;
	if (all_blobs) gf_list_del_item(all_blobs, blob);
}

GF_Blob *gf_blob_from_url(const char *blob_url)
{
	GF_Blob *blob = NULL;
	if (strncmp(blob_url, "gmem://", 7)) return NULL;
	if (sscanf(blob_url, "gmem://%p", &blob) != 1) return NULL;
	if (!blob)
		return NULL;
	if (gf_list_find(all_blobs, blob)<0)
		return NULL;
	return blob;
}

void gf_init_global_config(const char *profile);
void gf_uninit_global_config(Bool discard_config);

static GF_Config *gpac_lang_file = NULL;
static const char *gpac_lang_code = NULL;

GF_Config *gf_sys_get_lang_file()
{
	const char *opt = gf_opts_get_key("core", "lang");
	char szSharedPath[GF_MAX_PATH];
	if (!opt) return NULL;

	if (gpac_lang_code && strcmp(gpac_lang_code, opt)) {
		gf_cfg_del(gpac_lang_file);
		gpac_lang_file = NULL;
	}
	gpac_lang_code = opt;

	if (gpac_lang_file) return gpac_lang_file;

	if (! gf_opts_default_shared_directory(szSharedPath)) return NULL;
	strcat(szSharedPath, "/lang/");
	strcat(szSharedPath, opt);
	strcat(szSharedPath, ".txt");
	if (!gf_file_exists(szSharedPath)) return NULL;

	gpac_lang_file = gf_cfg_new(NULL, szSharedPath);

	return gpac_lang_file;
}


GF_EXPORT
const char *gf_sys_localized(const char *sec_name, const char *key_name, const char *def_val)
{
	GF_Config *lcfg = gf_sys_get_lang_file();
	if (!lcfg) return def_val;

	const char *opt = gf_cfg_get_key(lcfg, sec_name, key_name);
	if (opt) return opt;
	return def_val;
}

static void gf_sys_refresh_cache()
{
	u32 i, count;
	count = gf_opts_get_section_count();
	for (i=0; i<count; i++) {
		const char *opt;
		u32 sec, frac, exp;
		Bool force_delete;
		const char *file;
		const char *name = gf_opts_get_section_name(i);
		if (strncmp(name, "@cache=", 7)) continue;

		file = gf_opts_get_key(name, "cacheFile");
		opt = gf_opts_get_key(name, "expireAfterNTP");
		if (!opt) {
			if (file) gf_file_delete((char*) file);
			gf_opts_del_section(name);
			i--;
			count--;
			continue;
		}

		force_delete = 0;
		if (file && gf_file_exists(file)) {
			force_delete = 1;
		}
		sscanf(opt, "%u", &exp);
		gf_net_get_ntp(&sec, &frac);
		if (exp && (exp<sec)) force_delete=1;

		if (force_delete) {
			if (file) gf_file_delete(file);

			gf_opts_del_section(name);
			i--;
			count--;
			continue;
		}
	}
}

GF_EXPORT
GF_Err gf_sys_init(GF_MemTrackerType mem_tracker_type, const char *profile)
{
	if (!sys_init) {
#if defined (WIN32)
#if defined(_WIN32_WCE)
		MEMORYSTATUS ms;
#else
		SYSTEM_INFO sysinfo;
#endif
#endif

		if (mem_tracker_type!=GF_MemTrackerNone) {
#ifdef GPAC_MEMORY_TRACKING
            gf_mem_enable_tracker( (mem_tracker_type==GF_MemTrackerBackTrace) ? GF_TRUE : GF_FALSE);
#endif
		}
#ifndef GPAC_DISABLE_LOG
		/*by default log subsystem is initialized to error on all tools, and info on console to debug scripts*/
		gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_WARNING);
		gf_log_set_tool_level(GF_LOG_APP, GF_LOG_INFO);
		gf_log_set_tool_level(GF_LOG_CONSOLE, GF_LOG_INFO);
#endif


#if defined(__sh__)
		/* Round all denormalized floatting point number to 0.0 */
		sh4_change_fpscr(0,SH4_FPSCR_DN) ;
#endif

#if defined(WIN32)
		frequency.QuadPart = 0;
		/*clock setup*/
		if (QueryPerformanceFrequency(&frequency)) {
			QueryPerformanceCounter(&init_counter);
			OS_GetSysClock = OS_GetSysClockHIGHRES;
			OS_GetSysClockHR = OS_GetSysClockHIGHRES_FULL;
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[core] using WIN32 performance timer\n"));
		} else {
			OS_GetSysClock = OS_GetSysClockNORMAL;
			OS_GetSysClockHR = OS_GetSysClockNORMAL_FULL;
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[core] using WIN32 regular timer\n"));
		}

#ifndef _WIN32_WCE
		timeBeginPeriod(1);
#endif

		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[core] checking for run-time info tools"));
#if defined(_WIN32_WCE)
		last_total_k_u_time = last_process_k_u_time = 0;
		last_update_time = 0;
		memset(&the_rti, 0, sizeof(GF_SystemRTInfo));
		the_rti.pid = GetCurrentProcessId();
		the_rti.nb_cores = 1;
		GlobalMemoryStatus(&ms);
		mem_usage_at_startup = ms.dwAvailPhys;
#else
		/*cpu usage tools are buried in win32 dlls...*/
		MyGetSystemTimes = (NTGetSystemTimes) GetProcAddress(GetModuleHandle("kernel32.dll"), "GetSystemTimes");
		if (!MyGetSystemTimes) {
			MyQuerySystemInfo = (NTQuerySystemInfo) GetProcAddress(GetModuleHandle("ntdll.dll"), "NtQuerySystemInformation");
			if (MyQuerySystemInfo) {
				GF_LOG(GF_LOG_INFO, GF_LOG_CORE, (" - CPU: QuerySystemInformation"));
			}
		} else {
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, (" - CPU: GetSystemsTimes"));
		}
		psapi_hinst = LoadLibrary("psapi.dll");
		MyGetProcessMemoryInfo = (NTGetProcessMemoryInfo) GetProcAddress(psapi_hinst, "GetProcessMemoryInfo");
		if (MyGetProcessMemoryInfo) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, (" - memory: GetProcessMemoryInfo"));
		}
		last_process_k_u_time = last_proc_idle_time = last_proc_k_u_time = 0;
		last_update_time = 0;
		memset(&the_rti, 0, sizeof(GF_SystemRTInfo));
		the_rti.pid = GetCurrentProcessId();

		GetSystemInfo( &sysinfo );
		the_rti.nb_cores = sysinfo.dwNumberOfProcessors;
#endif
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("\n"));

#else
		/*linux threads and OSX...*/
		last_process_k_u_time = 0;
		last_cpu_u_k_time = last_cpu_idle_time = 0;
		last_update_time = 0;
		memset(&the_rti, 0, sizeof(GF_SystemRTInfo));
		the_rti.pid = getpid();
		the_rti.nb_cores = (u32) sysconf( _SC_NPROCESSORS_ONLN );
		sys_start_time = gf_sys_clock();
		sys_start_time_hr = gf_sys_clock_high_res();
#endif
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[core] process id %d\n", the_rti.pid));

#ifndef _WIN32_WCE
		setlocale( LC_NUMERIC, "C" );
#endif


		logs_mx = gf_mx_new("Logs");

		gf_rand_init(GF_FALSE);

		gf_init_global_config(profile);

		gf_sys_refresh_cache();

	}
	sys_init += 1;


	/*init RTI stats*/
	if (!memory_at_gpac_startup) {
		GF_SystemRTInfo rti;
		if (gf_sys_get_rti(500, &rti, GF_RTI_SYSTEM_MEMORY_ONLY)) {
			memory_at_gpac_startup = rti.physical_memory_avail;
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[core] System init OK - process id %d - %d MB physical RAM - %d cores\n", rti.pid, (u32) (rti.physical_memory/1024/1024), rti.nb_cores));
		} else {
			memory_at_gpac_startup = 0;
		}
	}

	return GF_OK;
}

GF_EXPORT
void gf_sys_close()
{
	if (sys_init > 0) {
		void gf_sys_cleanup_help();

		GF_Mutex *old_log_mx;
		sys_init --;
		if (sys_init) return;
		/*prevent any call*/
		last_update_time = 0xFFFFFFFF;

#if defined(WIN32) && !defined(_WIN32_WCE)
		timeEndPeriod(1);

		MyGetSystemTimes = NULL;
		MyGetProcessMemoryInfo = NULL;
		MyQuerySystemInfo = NULL;
		if (psapi_hinst) FreeLibrary(psapi_hinst);
		psapi_hinst = NULL;
#endif

		gf_sys_enable_remotery(GF_FALSE, GF_TRUE);

#ifdef GPAC_HAS_QJS
		void gf_js_delete_runtime();
		gf_js_delete_runtime();
#endif

		gf_uninit_global_config(gpac_discard_config);

#ifndef GPAC_DISABLE_LOG
		if (gpac_log_file) {
			gf_fclose(gpac_log_file);
			gpac_log_file = NULL;
		}
#endif
		if (gpac_lang_file) gf_cfg_del(gpac_lang_file);
		gpac_lang_file = NULL;

		gf_sys_cleanup_help();

		old_log_mx = logs_mx;
		logs_mx = NULL;
		gf_mx_del(old_log_mx);

		if (gpac_argv_state) {
			gf_free(gpac_argv_state);
			gpac_argv_state = NULL;
		}
		gpac_argc = 0;
		gpac_argv = NULL;
		memory_at_gpac_startup = 0;
		gpac_test_mode = gpac_old_arch = gpac_discard_config = GF_FALSE;

		gf_list_del(all_blobs);
		all_blobs = NULL;

	}
}

#ifdef GPAC_MEMORY_TRACKING
extern size_t gpac_allocated_memory;
extern size_t gpac_nb_alloc_blocs;
#endif

/*CPU and Memory Usage*/
#ifdef WIN32

Bool gf_sys_get_rti_os(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
#if defined(_WIN32_WCE)
	THREADENTRY32 tentry;
	u64 total_cpu_time, process_cpu_time;
	DWORD orig_perm;
#endif
	MEMORYSTATUS ms;
	u64 creation, exit, kernel, user, process_k_u_time, proc_idle_time, proc_k_u_time;
	u32 entry_time;
	HANDLE hSnapShot;

	assert(sys_init);

	if (!rti) return GF_FALSE;

	proc_idle_time = proc_k_u_time = process_k_u_time = 0;

	entry_time = gf_sys_clock();
	if (last_update_time && (entry_time - last_update_time < refresh_time_ms)) {
		memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
		return GF_FALSE;
	}

	if (flags & GF_RTI_SYSTEM_MEMORY_ONLY) {
		memset(rti, 0, sizeof(GF_SystemRTInfo));
		rti->sampling_instant = last_update_time;
		GlobalMemoryStatus(&ms);
		rti->physical_memory = ms.dwTotalPhys;
		rti->physical_memory_avail = ms.dwAvailPhys;
#ifdef GPAC_MEMORY_TRACKING
		rti->gpac_memory = (u64) gpac_allocated_memory;
#endif
		return GF_TRUE;
	}

#if defined (_WIN32_WCE)

	total_cpu_time = process_cpu_time = 0;

	/*get a snapshot of all running threads*/
	orig_perm = GetCurrentPermissions();
	SetProcPermissions(0xFFFFFFFF);
	hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
	if (hSnapShot) {
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
	}

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

#else
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
		if (!hSnapShot) return GF_FALSE;
		pentry.dwSize = sizeof(PROCESSENTRY32);
		if (Process32First(hSnapShot, &pentry)) {
			do {
				HANDLE procH = NULL;
				if (pentry.th32ProcessID) procH = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, pentry.th32ProcessID);
				if (procH && GetProcessTimes(procH, (FILETIME *) &creation, (FILETIME *) &exit, (FILETIME *) &kernel, (FILETIME *) &user) ) {
					user += kernel;
					proc_k_u_time += user;
					if (pentry.th32ProcessID==the_rti.pid) {
						process_k_u_time = user;
						//nb_threads = pentry.cntThreads;
					}
				}
				if (procH) CloseHandle(procH);
			} while (Process32Next(hSnapShot, &pentry));
		}
		CloseHandle(hSnapShot);
		proc_k_u_time /= 10;
	}


	if (!process_k_u_time) {
		HANDLE procH = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, the_rti.pid);
		if (procH && GetProcessTimes(procH, (FILETIME *) &creation, (FILETIME *) &exit, (FILETIME *) &kernel, (FILETIME *) &user) ) {
			process_k_u_time = user + kernel;
		}
		if (procH) CloseHandle(procH);
		if (!process_k_u_time) return GF_FALSE;
	}
	process_k_u_time /= 10;

	/*this won't cost a lot*/
	if (MyGetProcessMemoryInfo) {
		PROCESS_MEMORY_COUNTERS pmc;
		HANDLE procH = OpenProcess(PROCESS_QUERY_INFORMATION, FALSE, the_rti.pid);
		MyGetProcessMemoryInfo(procH, &pmc, sizeof (pmc));
		the_rti.process_memory = pmc.WorkingSetSize;
		if (procH) CloseHandle(procH);
	}
	/*THIS IS VERY HEAVY (eats up mem and time) - only perform if requested*/
	else if (flags & GF_RTI_PROCESS_MEMORY) {
		HEAPLIST32 hlentry;
		HEAPENTRY32 hentry;
		the_rti.process_memory = 0;
		hlentry.dwSize = sizeof(HEAPLIST32);
		hSnapShot = CreateToolhelp32Snapshot(TH32CS_SNAPHEAPLIST, the_rti.pid);
		if (hSnapShot && Heap32ListFirst(hSnapShot, &hlentry)) {
			do {
				hentry.dwSize = sizeof(hentry);
				if (Heap32First(&hentry, hlentry.th32ProcessID, hlentry.th32HeapID)) {
					do {
						the_rti.process_memory += hentry.dwBlockSize;
					} while (Heap32Next(&hentry));
				}
			} while (Heap32ListNext(hSnapShot, &hlentry));
		}
		CloseHandle(hSnapShot);
	}
#endif

	the_rti.sampling_instant = last_update_time;

	if (last_update_time) {
		the_rti.sampling_period_duration = entry_time - last_update_time;
		the_rti.process_cpu_time_diff = (u32) ((process_k_u_time - last_process_k_u_time)/1000);

#if defined(_WIN32_WCE)
		the_rti.total_cpu_time_diff = (u32) ((total_cpu_time - last_total_k_u_time)/1000);
		/*we're not that accurate....*/
		if (the_rti.total_cpu_time_diff > the_rti.sampling_period_duration)
			the_rti.sampling_period_duration = the_rti.total_cpu_time_diff;

		/*rough values*/
		the_rti.cpu_idle_time = the_rti.sampling_period_duration - the_rti.total_cpu_time_diff;
		if (!the_rti.sampling_period_duration) the_rti.sampling_period_duration=1;
		the_rti.total_cpu_usage = (u32) (100 * the_rti.total_cpu_time_diff / the_rti.sampling_period_duration);
		if (the_rti.total_cpu_time_diff + the_rti.cpu_idle_time==0) the_rti.total_cpu_time_diff ++;
		the_rti.process_cpu_usage = (u32) (100*the_rti.process_cpu_time_diff / (the_rti.total_cpu_time_diff + the_rti.cpu_idle_time) );

#else
		/*oops, we have no choice but to assume 100% cpu usage during this period*/
		if (!proc_k_u_time) {
			the_rti.total_cpu_time_diff = the_rti.sampling_period_duration;
			proc_k_u_time = last_proc_k_u_time + the_rti.sampling_period_duration;
			the_rti.cpu_idle_time = 0;
			the_rti.total_cpu_usage = 100;
			if (the_rti.sampling_period_duration)
				the_rti.process_cpu_usage = (u32) (100*the_rti.process_cpu_time_diff / the_rti.sampling_period_duration);
		} else {
			u64 samp_sys_time, idle;
			the_rti.total_cpu_time_diff = (u32) ((proc_k_u_time - last_proc_k_u_time)/1000);

			/*we're not that accurate....*/
			if (the_rti.total_cpu_time_diff > the_rti.sampling_period_duration) {
				the_rti.sampling_period_duration = the_rti.total_cpu_time_diff;
			}

			if (!proc_idle_time)
				proc_idle_time = last_proc_idle_time + (the_rti.sampling_period_duration - the_rti.total_cpu_time_diff);

			samp_sys_time = proc_k_u_time - last_proc_k_u_time;
			idle = proc_idle_time - last_proc_idle_time;
			the_rti.cpu_idle_time = (u32) (idle/1000);
			if (samp_sys_time) {
				the_rti.total_cpu_usage = (u32) ( (samp_sys_time - idle) / (samp_sys_time / 100) );
				the_rti.process_cpu_usage = (u32) (100*the_rti.process_cpu_time_diff / (samp_sys_time/1000));
			}
		}
#endif
	}
	last_update_time = entry_time;
	last_process_k_u_time = process_k_u_time;

	GlobalMemoryStatus(&ms);
	the_rti.physical_memory = ms.dwTotalPhys;
#ifdef GPAC_MEMORY_TRACKING
	the_rti.gpac_memory = (u64) gpac_allocated_memory;
#endif
	the_rti.physical_memory_avail = ms.dwAvailPhys;

#if defined(_WIN32_WCE)
	last_total_k_u_time = total_cpu_time;
	if (!the_rti.process_memory) the_rti.process_memory = mem_usage_at_startup - ms.dwAvailPhys;
#else
	last_proc_idle_time = proc_idle_time;
	last_proc_k_u_time = proc_k_u_time;
#endif

	if (!the_rti.gpac_memory) the_rti.gpac_memory = the_rti.process_memory;

	memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
	return GF_TRUE;
}


#elif defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_CONFIG_IOS)

#include <sys/types.h>
#include <sys/sysctl.h>
#include <sys/vmmeter.h>
#include <mach/mach_init.h>
#include <mach/mach_host.h>
#include <mach/mach_port.h>
#include <mach/mach_traps.h>
#include <mach/task_info.h>
#include <mach/thread_info.h>
#include <mach/thread_act.h>
#include <mach/vm_region.h>
#include <mach/vm_map.h>
#include <mach/task.h>
#if __ENVIRONMENT_MAC_OS_X_VERSION_MIN_REQUIRED__ >= 1060
#include <mach/shared_region.h>
#else
#include <mach/shared_memory_server.h>
#endif
#include <mach/mach_error.h>

static u64 total_physical_memory = 0;

Bool gf_sys_get_rti_os(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
	size_t length;
	u32 entry_time, i, percent;
	int mib[6];
	u64 result;
	int pagesize;
	u64 process_u_k_time;
	double utime, stime;
	vm_statistics_data_t vmstat;
	task_t task;
	kern_return_t error;
	thread_array_t thread_table;
	unsigned table_size;
	thread_basic_info_t thi;
	thread_basic_info_data_t thi_data;
	struct task_basic_info ti;
	mach_msg_type_number_t count = HOST_VM_INFO_COUNT, size = sizeof(ti);

	entry_time = gf_sys_clock();
	if (last_update_time && (entry_time - last_update_time < refresh_time_ms)) {
		memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
		return 0;
	}

	mib[0] = CTL_HW;
	mib[1] = HW_PAGESIZE;
	length = sizeof(pagesize);
	if (sysctl(mib, 2, &pagesize, &length, NULL, 0) < 0) {
		return 0;
	}

	if (host_statistics(mach_host_self(), HOST_VM_INFO, (host_info_t)&vmstat, &count) != KERN_SUCCESS) {
		return 0;
	}

	the_rti.physical_memory = (vmstat.wire_count + vmstat.active_count + vmstat.inactive_count + vmstat.free_count)* pagesize;
	the_rti.physical_memory_avail = vmstat.free_count * pagesize;

	if (!total_physical_memory) {
		mib[0] = CTL_HW;
		mib[1] = HW_MEMSIZE;
		length = sizeof(u64);
		if (sysctl(mib, 2, &result, &length, NULL, 0) >= 0) {
			total_physical_memory = result;
		}
	}
	the_rti.physical_memory = total_physical_memory;

	error = task_for_pid(mach_task_self(), the_rti.pid, &task);
	if (error) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] Cannot get process task for PID %d: error %d\n", the_rti.pid, error));
		return 0;
	}

	error = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)&ti, &size);
	if (error) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] Cannot get process task info (PID %d): error %d\n", the_rti.pid, error));
		return 0;
	}

	percent = 0;
	utime = ti.user_time.seconds + ti.user_time.microseconds * 1e-6;
	stime = ti.system_time.seconds + ti.system_time.microseconds * 1e-6;
	error = task_threads(task, &thread_table, &table_size);
	if (error != KERN_SUCCESS) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] Cannot get threads task for PID %d: error %d\n", the_rti.pid, error));
		return 0;
	}
	thi = &thi_data;
	for (i = 0; i != table_size; ++i) {
		count = THREAD_BASIC_INFO_COUNT;
		error = thread_info(thread_table[i], THREAD_BASIC_INFO, (thread_info_t)thi, &count);
		if (error != KERN_SUCCESS) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[RTI] Unexpected thread_info error for process %d: %s\n", the_rti.pid, mach_error_string(error) ));
			break;
		}
		if ((thi->flags & TH_FLAGS_IDLE) == 0) {
			utime += thi->user_time.seconds + thi->user_time.microseconds * 1e-6;
			stime += thi->system_time.seconds + thi->system_time.microseconds * 1e-6;
			percent +=  (u32) (100 * (double)thi->cpu_usage / TH_USAGE_SCALE);
		}
	}
	vm_deallocate(mach_task_self(), (vm_offset_t)thread_table, table_size * sizeof(thread_array_t));
	mach_port_deallocate(mach_task_self(), task);

	process_u_k_time = utime + stime;

	the_rti.sampling_instant = last_update_time;

	if (last_update_time) {
		the_rti.sampling_period_duration = (entry_time - last_update_time);
		the_rti.process_cpu_time_diff = (process_u_k_time - last_process_k_u_time) * 10;

		the_rti.total_cpu_time_diff = the_rti.sampling_period_duration;
		/*TODO*/
		the_rti.cpu_idle_time = 0;
		the_rti.total_cpu_usage = 0;
		if (!the_rti.process_cpu_time_diff) the_rti.process_cpu_time_diff = the_rti.total_cpu_time_diff;

		the_rti.process_cpu_usage = percent;
	} else {
		mem_at_startup = the_rti.physical_memory_avail;
	}
	the_rti.process_memory = mem_at_startup - the_rti.physical_memory_avail;

#ifdef GPAC_MEMORY_TRACKING
	the_rti.gpac_memory = gpac_allocated_memory;
#endif

	last_process_k_u_time = process_u_k_time;
	last_cpu_idle_time = 0;
	last_update_time = entry_time;
	memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
	return 1;
}

//ios
#elif defined(GPAC_CONFIG_IOS)

#import <mach/mach.h>
#import <assert.h>

Bool gf_sys_get_rti_os(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
	u64 u_k_time = 0;
    kern_return_t kr;
    task_info_data_t tinfo;
    mach_msg_type_number_t task_info_count;
    task_basic_info_t basic_info;
    thread_array_t thread_list;
    mach_msg_type_number_t thread_count;
    thread_info_data_t thinfo;
    mach_msg_type_number_t thread_info_count;
    thread_basic_info_t basic_info_th;
    u32 j, tot_cpu = 0, nb_threads = 0;

	u32 entry_time = gf_sys_clock();
	if (last_update_time && (entry_time - last_update_time < refresh_time_ms)) {
		memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
		return GF_FALSE;
	}

    task_info_count = TASK_INFO_MAX;
    kr = task_info(mach_task_self(), TASK_BASIC_INFO, (task_info_t)tinfo, &task_info_count);
    if (kr != KERN_SUCCESS) {
        return GF_FALSE;
    }

    basic_info = (task_basic_info_t)tinfo;
    the_rti.gpac_memory = the_rti.process_memory = basic_info->resident_size;

    // get threads in the task
    kr = task_threads(mach_task_self(), &thread_list, &thread_count);
    if (kr != KERN_SUCCESS) {
        return GF_FALSE;
    }
    if (thread_count > 0)
        nb_threads = (u32) thread_count;

    for (j = 0; j < nb_threads; j++)   {
        thread_info_count = THREAD_INFO_MAX;
        kr = thread_info(thread_list[j], THREAD_BASIC_INFO,
                         (thread_info_t)thinfo, &thread_info_count);
        if (kr != KERN_SUCCESS) {
			vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
            return GF_FALSE;
        }

        basic_info_th = (thread_basic_info_t)thinfo;

        if (!(basic_info_th->flags & TH_FLAGS_IDLE)) {
            u64 tot_sec = basic_info_th->user_time.seconds + basic_info_th->system_time.seconds;
            tot_sec *= 1000000;
            tot_sec += basic_info_th->user_time.microseconds + basic_info_th->system_time.microseconds;
            u_k_time += tot_sec;
			tot_cpu += basic_info_th->cpu_usage;
        }

    } // for each thread

    the_rti.process_cpu_usage = (tot_cpu * 100) / TH_USAGE_SCALE;
	the_rti.total_cpu_usage = the_rti.process_cpu_usage;

    kr = vm_deallocate(mach_task_self(), (vm_offset_t)thread_list, thread_count * sizeof(thread_t));
    assert(kr == KERN_SUCCESS);


	if (last_update_time) {
		the_rti.sampling_period_duration = (entry_time - last_update_time);
		the_rti.process_cpu_time_diff = 0;
		/*move to ms*/
		the_rti.total_cpu_time_diff = (u32) (u_k_time - last_cpu_u_k_time) / 1000;

		/*we're not that accurate....*/
		if (the_rti.total_cpu_time_diff > the_rti.sampling_period_duration)
			the_rti.sampling_period_duration = the_rti.total_cpu_time_diff;
	} else {
		mem_at_startup = the_rti.physical_memory_avail;
	}

#ifdef GPAC_MEMORY_TRACKING
	the_rti.gpac_memory = gpac_allocated_memory;
#endif
	last_cpu_u_k_time = u_k_time;
	last_update_time = entry_time;
	memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));

	last_update_time = entry_time;
	return GF_TRUE;
}

#else


Bool gf_sys_get_rti_os(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
	u32 entry_time;
	u64 process_u_k_time;
	u32 u_k_time, idle_time;
#if 0
	char szProc[100];
#endif
	FILE *f;

	assert(sys_init);

	entry_time = gf_sys_clock();
	if (last_update_time && (entry_time - last_update_time < refresh_time_ms)) {
		memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
		return GF_FALSE;
	}
	u_k_time = idle_time = 0;
	f = gf_fopen("/proc/stat", "r");
	if (f) {
		char line[2048];
		u32 k_time, nice_time, u_time;
		if (gf_fgets(line, 128, f) != NULL) {
			if (sscanf(line, "cpu  %u %u %u %u\n", &u_time, &k_time, &nice_time, &idle_time) == 4) {
				u_k_time = u_time + k_time + nice_time;
			}
		}
		gf_fclose(f);
	}
	process_u_k_time = 0;
	the_rti.process_memory = 0;

	/*FIXME? under LinuxThreads this will only fetch stats for the calling thread, we would have to enumerate /proc to get
	the complete CPU usage of all threads of the process...*/
#if 0
	sprintf(szProc, "/proc/%d/stat", the_rti.pid);
	f = gf_fopen(szProc, "r");
	if (f) {
		gf_fflush(f);
		if (gf_fgets(line, 2048, f) != NULL) {
			char state;
			char *start;
			long cutime, cstime, priority, nice, itrealvalue, rss;
			int exit_signal, processor;
			unsigned long flags, minflt, cminflt, majflt, cmajflt, utime, stime,starttime, vsize, rlim, startcode, endcode, startstack, kstkesp, kstkeip, signal, blocked, sigignore, sigcatch, wchan, nswap, cnswap, rem;
			int ppid, pgrp ,session, tty_nr, tty_pgrp, res;
			start = strchr(line, ')');
			if (start) start += 2;
			else {
				start = strchr(line, ' ');
				start++;
			}
			res = sscanf(start,"%c %d %d %d %d %d %lu %lu %lu %lu \
%lu %lu %lu %ld %ld %ld %ld %ld %ld %lu \
%lu %ld %lu %lu %lu %lu %lu %lu %lu %lu \
%lu %lu %lu %lu %lu %d %d",
			             &state, &ppid, &pgrp, &session, &tty_nr, &tty_pgrp, &flags, &minflt, &cminflt, &majflt,
			             &cmajflt, &utime, &stime, &cutime, &cstime, &priority, &nice, &itrealvalue, &rem, &starttime,
			             &vsize, &rss, &rlim, &startcode, &endcode, &startstack, &kstkesp, &kstkeip, &signal, &blocked,
			             &sigignore, &sigcatch, &wchan, &nswap, &cnswap, &exit_signal, &processor);

			if (res) process_u_k_time = (u64) (cutime + cstime);
			else {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] PROC %s parse error\n", szProc));
			}
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] error reading pid/stat\n\n", szProc));
		}
		gf_fclose(f);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] cannot open %s\n", szProc));
	}
	sprintf(szProc, "/proc/%d/status", the_rti.pid);
	f = gf_fopen(szProc, "r");
	if (f) {
		while (gf_fgets(line, 1024, f) != NULL) {
			if (!strnicmp(line, "VmSize:", 7)) {
				sscanf(line, "VmSize: %"LLD" kB",  &the_rti.process_memory);
				the_rti.process_memory *= 1024;
			}
		}
		gf_fclose(f);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] cannot open %s\n", szProc));
	}
#endif


#ifndef GPAC_CONFIG_IOS
	the_rti.physical_memory = the_rti.physical_memory_avail = 0;
	f = gf_fopen("/proc/meminfo", "r");
	if (f) {
		char line[2048];
		while (gf_fgets(line, 1024, f) != NULL) {
			if (!strnicmp(line, "MemTotal:", 9)) {
				sscanf(line, "MemTotal: " LLU " kB",  &the_rti.physical_memory);
				the_rti.physical_memory *= 1024;
			} else if (!strnicmp(line, "MemFree:", 8)) {
				sscanf(line, "MemFree: " LLU " kB",  &the_rti.physical_memory_avail);
				the_rti.physical_memory_avail *= 1024;
				break;
			}
		}
		gf_fclose(f);
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] cannot open /proc/meminfo\n"));
	}
#endif

	the_rti.sampling_instant = last_update_time;

	if (last_update_time) {
		the_rti.sampling_period_duration = (entry_time - last_update_time);
		the_rti.process_cpu_time_diff = (u32) (process_u_k_time - last_process_k_u_time) * 10;

		/*oops, we have no choice but to assume 100% cpu usage during this period*/
		if (!u_k_time) {
			the_rti.total_cpu_time_diff = the_rti.sampling_period_duration;
			u_k_time = (u32) (last_cpu_u_k_time + the_rti.sampling_period_duration);
			the_rti.cpu_idle_time = 0;
			the_rti.total_cpu_usage = 100;
			if (!the_rti.process_cpu_time_diff) the_rti.process_cpu_time_diff = the_rti.total_cpu_time_diff;
			the_rti.process_cpu_usage = (u32) ( 100 *  the_rti.process_cpu_time_diff / the_rti.sampling_period_duration);
		} else {
			u64 samp_sys_time;
			/*move to ms (/proc/stat gives times in 100 ms unit*/
			the_rti.total_cpu_time_diff = (u32) (u_k_time - last_cpu_u_k_time)*10;

			/*we're not that accurate....*/
			if (the_rti.total_cpu_time_diff > the_rti.sampling_period_duration)
				the_rti.sampling_period_duration = the_rti.total_cpu_time_diff;

			if (!idle_time) idle_time = (the_rti.sampling_period_duration - the_rti.total_cpu_time_diff)/10;
			samp_sys_time = u_k_time - last_cpu_u_k_time;
			the_rti.cpu_idle_time = (u32) (idle_time - last_cpu_idle_time);
			if (the_rti.cpu_idle_time + samp_sys_time > 0)
				the_rti.total_cpu_usage = (u32) ( 100 * samp_sys_time / (the_rti.cpu_idle_time + samp_sys_time ) );
			else
				the_rti.total_cpu_usage = 0;
			/*move to ms (/proc/stat gives times in 100 ms unit*/
			the_rti.cpu_idle_time *= 10;
			if (!the_rti.process_cpu_time_diff) the_rti.process_cpu_time_diff = the_rti.total_cpu_time_diff;
			if (the_rti.cpu_idle_time + 10*samp_sys_time > 0)
				the_rti.process_cpu_usage = (u32) ( 100 *  the_rti.process_cpu_time_diff / (the_rti.cpu_idle_time + 10*samp_sys_time ) );
			else
				the_rti.process_cpu_usage = 0;
		}
	} else {
		mem_at_startup = the_rti.physical_memory_avail;
	}
	the_rti.process_memory = mem_at_startup - the_rti.physical_memory_avail;
#ifdef GPAC_MEMORY_TRACKING
	the_rti.gpac_memory = gpac_allocated_memory;
#endif
	last_process_k_u_time = process_u_k_time;
	last_cpu_idle_time = idle_time;
	last_cpu_u_k_time = u_k_time;
	last_update_time = entry_time;
	memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
	return GF_TRUE;
}

#endif

GF_EXPORT
Bool gf_sys_get_rti(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
	Bool res = gf_sys_get_rti_os(refresh_time_ms, rti, flags);
	if (res) {
		if (!rti->process_memory) rti->process_memory = memory_at_gpac_startup - rti->physical_memory_avail;
		if (!rti->gpac_memory) rti->gpac_memory = memory_at_gpac_startup - rti->physical_memory_avail;
	}
	return res;
}

static char szCacheDir[GF_MAX_PATH];

const char * gf_get_default_cache_directory_ex(Bool do_create)
{
	const char *cache_dir;
	char root_tmp[GF_MAX_PATH];
	size_t len;

	cache_dir = gf_opts_get_key("core", "cache");
	if (cache_dir) return cache_dir;

#ifdef _WIN32_WCE
	strcpy(szCacheDir, "\\windows\\temp" );
#elif defined(WIN32)
	GetTempPath(GF_MAX_PATH, szCacheDir);
#elif defined(GPAC_CONFIG_ANDROID)
	strcpy(szCacheDir, "/data/data/io.gpac.gpac/cache");
#else
	strcpy(szCacheDir, "/tmp");
#endif

	strcpy(root_tmp, szCacheDir);

	len = strlen(szCacheDir);
	if (szCacheDir[len-1] != GF_PATH_SEPARATOR) {
		szCacheDir[len] = GF_PATH_SEPARATOR;
		szCacheDir[len+1] = 0;
	}

	strcat(szCacheDir, "gpac_cache");

	if (do_create && !gf_dir_exists(szCacheDir) && gf_mkdir(szCacheDir)!=GF_OK ) {
		strcpy(szCacheDir, root_tmp);
		return szCacheDir;
	}
	return szCacheDir;
}

GF_EXPORT
const char * gf_get_default_cache_directory()
{
	return gf_get_default_cache_directory_ex(GF_TRUE);
}


GF_EXPORT
Bool gf_sys_get_battery_state(Bool *onBattery, u32 *onCharge, u32*level, u32 *batteryLifeTime, u32 *batteryFullLifeTime)
{
#if defined(_WIN32_WCE)
	SYSTEM_POWER_STATUS_EX sps;
	GetSystemPowerStatusEx(&sps, 0);
	if (onBattery) *onBattery = sps.ACLineStatus ? 0 : 1;
	if (onCharge) *onCharge = (sps.BatteryFlag & BATTERY_FLAG_CHARGING) ? 1 : 0;
	if (level) *level = sps.BatteryLifePercent;
	if (batteryLifeTime) *batteryLifeTime = sps.BatteryLifeTime;
	if (batteryFullLifeTime) *batteryFullLifeTime = sps.BatteryFullLifeTime;
#elif defined(WIN32)
	SYSTEM_POWER_STATUS sps;
	GetSystemPowerStatus(&sps);
	if (onBattery) *onBattery = sps.ACLineStatus ? GF_FALSE : GF_TRUE;
	if (onCharge) *onCharge = (sps.BatteryFlag & BATTERY_FLAG_CHARGING) ? 1 : 0;
	if (level) *level = sps.BatteryLifePercent;
	if (batteryLifeTime) *batteryLifeTime = sps.BatteryLifeTime;
	if (batteryFullLifeTime) *batteryFullLifeTime = sps.BatteryFullLifeTime;
#endif
	return GF_TRUE;
}


#if 0 //global lock currently not used

struct GF_GlobalLock {
	const char * resourceName;
};


#ifndef WIN32
#define CPF_CLOEXEC 1

#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

struct _GF_GlobalLock_opaque {
	char * resourceName;
	char * pidFile;
	int fd;
};

GF_GlobalLock * gf_create_PID_file( const char * resourceName )
{
	const char * prefix = "/gpac_lock_";
	const char * dir = gf_get_default_cache_directory();
	char * pidfile;
	int flags;
	int status;
	pidfile = gf_malloc(strlen(dir)+strlen(prefix)+strlen(resourceName)+1);
	strcpy(pidfile, dir);
	strcat(pidfile, prefix);
	/* Use only valid names for file */
	{
		const char *res;
		char * pid = &(pidfile[strlen(pidfile)]);
		for (res = resourceName; *res ; res++) {
			if (*res >= 'A' && *res <= 'z')
				*pid = * res;
			else
				*pid = '_';
			pid++;
		}
		*pid = '\0';
	}
	int fd = open(pidfile, O_RDWR | O_CREAT, S_IRUSR | S_IWUSR);
	if (fd == -1)
		goto exit;
	/* Get the flags */
	flags = fcntl(fd, F_GETFD);
	if (flags == -1) {
		goto exit;
	}
	/* Set FD_CLOEXEC, so exclusive lock will be removed on exit, so even if GPAC crashes,
	* lock will be allowed for next instance */
	flags |= FD_CLOEXEC;
	/* Now, update the flags */
	if (fcntl(fd, F_SETFD, flags) == -1) {
		goto exit;
	}

	/* Now, we try to lock the file */
	{
		struct flock fl;
		fl.l_type = F_WRLCK;
		fl.l_whence = SEEK_SET;
		fl.l_start = fl.l_len = 0;
		status = fcntl(fd, F_SETLK, &fl);
	}

	if (status == -1) {
		goto exit;
	}

	if (ftruncate(fd, 0) == -1) {
		goto exit;
	}
	/* Write the PID */
	{
		int sz = 100;
		char * buf = gf_malloc( sz );
		sz = snprintf(buf, sz, "%ld\n", (long) getpid());
		if (write(fd, buf, sz) != sz) {
			gf_free(buf);
			goto exit;
		}
	}
	sync();
	{
		GF_GlobalLock * lock = gf_malloc( sizeof(GF_GlobalLock));
		lock->resourceName = gf_strdup(resourceName);
		lock->pidFile = pidfile;
		lock->fd = fd;
		return lock;
	}
exit:
	if (fd >= 0)
		close(fd);
	return NULL;
}
#else /* WIN32 */
struct _GF_GlobalLock_opaque {
	char * resourceName;
	HANDLE hMutex; /*a named mutex is a system-mode object on windows*/
};
#endif

GF_EXPORT
GF_GlobalLock * gf_global_resource_lock(const char * resourceName) {
#ifdef WIN32
#ifdef _WIN32_WCE
	unsigned short sWResourceName[MAX_PATH];
#endif
	DWORD lastErr;
	GF_GlobalLock *lock = gf_malloc(sizeof(GF_GlobalLock));
	lock->resourceName = gf_strdup(resourceName);

	/*first ensure mutex is created*/
#ifdef _WIN32_WCE
	CE_CharToWide((char *)resourceName, sWResourceName);
	lock->hMutex = CreateMutex(NULL, TRUE, sWResourceName);
#else
	lock->hMutex = CreateMutex(NULL, TRUE, resourceName);
#endif
	lastErr = GetLastError();
	if (lastErr && lastErr == ERROR_ALREADY_EXISTS)
		return NULL;
	if (!lock->hMutex)
	{
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] Couldn't create mutex for global lock: %d\n", lastErr));
		return NULL;
	}

	/*then lock it*/
	switch (WaitForSingleObject(lock->hMutex, INFINITE)) {
	case WAIT_ABANDONED:
	case WAIT_TIMEOUT:
		assert(0); /*serious error: someone has modified the object elsewhere*/
		GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] Couldn't get the global lock\n"));
		gf_global_resource_unlock(lock);
		return NULL;
	}

	return lock;
#else /* WIN32 */
	return gf_create_PID_file(resourceName);
#endif /* WIN32 */
}

/*!
 * Unlock a previouly locked resource
\param lock The resource to unlock
\param GF_OK if evertything went fine
 */
GF_EXPORT
GF_Err gf_global_resource_unlock(GF_GlobalLock * lock) {
	if (!lock)
		return GF_BAD_PARAM;
#ifndef WIN32
	assert( lock->pidFile);
	close(lock->fd);
	if (unlink(lock->pidFile))
		perror("Failed to unlink lock file");
	gf_free(lock->pidFile);
	lock->pidFile = NULL;
	lock->fd = -1;
#else /* WIN32 */
	{
		/*MSDN: "The mutex object is destroyed when its last handle has been closed."*/
		BOOL ret = ReleaseMutex(lock->hMutex);
		if (!ret) {
			DWORD err = GetLastError();
			GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] Couldn't release mutex for global lock: %d\n", err));
		}
		ret = CloseHandle(lock->hMutex);
		if (!ret) {
			DWORD err = GetLastError();
			GF_LOG(GF_LOG_ERROR, GF_LOG_MUTEX, ("[Mutex] Couldn't destroy mutex for global lock: %d\n", err));
		}
	}
#endif
	if (lock->resourceName)
		gf_free(lock->resourceName);
	lock->resourceName = NULL;
	gf_free(lock);
	return GF_OK;
}

#endif //global lock uni-used


static u32 ntp_shift = GF_NTP_SEC_1900_TO_1970;

GF_EXPORT
void gf_net_set_ntp_shift(s32 shift)
{
	ntp_shift = GF_NTP_SEC_1900_TO_1970 + shift;
}

/*
		NTP tools
*/
GF_EXPORT
void gf_net_get_ntp(u32 *sec, u32 *frac)
{
	u64 frac_part;
	struct timeval now;
	gettimeofday(&now, NULL);
	if (sec) {
		*sec = (u32) (now.tv_sec) + ntp_shift;
	}

	if (frac) {
		frac_part = now.tv_usec * 0xFFFFFFFFULL;
		frac_part /= 1000000;
		*frac = (u32) ( frac_part );
	}
}

GF_EXPORT
u64 gf_net_get_ntp_ts()
{
	u64 res;
	u32 sec, frac;
	gf_net_get_ntp(&sec, &frac);
	res = sec;
	res<<= 32;
	res |= frac;
	return res;
}

GF_EXPORT
s32 gf_net_ntp_diff_ms(u64 ntp_a, u64 ntp_b)
{
	u32 ntp_a_s, ntp_a_f, ntp_b_s, ntp_b_f;
	s64 ntp_a_ms, ntp_b_ms;

	ntp_a_s = (ntp_a >> 32);
	ntp_a_f = (u32) (ntp_a & 0xFFFFFFFFULL);
	ntp_b_s = (ntp_b >> 32);
	ntp_b_f = (u32) (ntp_b & 0xFFFFFFFFULL);

	ntp_a_ms = ntp_a_s;
	ntp_a_ms *= 1000;
	ntp_a_ms += ((u64) ntp_a_f)*1000 / 0xFFFFFFFFULL;

	ntp_b_ms = ntp_b_s;
	ntp_b_ms *= 1000;
	ntp_b_ms += ((u64) ntp_b_f)*1000 / 0xFFFFFFFFULL;

	return (s32) (ntp_a_ms - ntp_b_ms);
}

GF_EXPORT
s32 gf_net_get_ntp_diff_ms(u64 ntp)
{
	u32 remote_s, remote_f, local_s, local_f;
	s64 local, remote;

	remote_s = (ntp >> 32);
	remote_f = (u32) (ntp & 0xFFFFFFFFULL);
	gf_net_get_ntp(&local_s, &local_f);

	local = local_s;
	local *= 1000;
	local += ((u64) local_f)*1000 / 0xFFFFFFFFULL;

	remote = remote_s;
	remote *= 1000;
	remote += ((u64) remote_f)*1000 / 0xFFFFFFFFULL;

	return (s32) (local - remote);
}

#if 0
/*!

Adds or remove a given amount of microseconds to an NTP timestamp
\param ntp NTP timestamp
\param usec microseconds to add/remove
\return adjusted NTP timestamp
 */
GF_EXPORT
u64 gf_net_add_usec(u64 ntp, s32 usec)
{
	u64 sec, frac;
	s64 usec_ntp;

	sec = (ntp >> 32);
	frac = (ntp & 0xFFFFFFFFULL);
	usec_ntp = (s64) ( frac*1000000 / 0xFFFFFFFFULL );
	usec_ntp += usec;
	while (usec_ntp > 1000000) {
		usec_ntp -= 1000000;
		sec += 1;
	}
	while (usec_ntp < 0) {
		usec_ntp += 1000000;
		sec -= 1;
	}
	ntp = ( usec_ntp * 0xFFFFFFFFULL / 1000000 ) & 0xFFFFFFFFULL;
	ntp |= (sec<<32);
	return ntp;
}
#endif


GF_EXPORT
u64 gf_net_get_ntp_ms()
{
	u32 sec, frac;
	u64 time_ms;
	Double msec;

	gf_net_get_ntp(&sec, &frac);
	time_ms = sec;
	time_ms *= 1000;
	msec = frac*1000.0;
	msec /= 0xFFFFFFFF;

	time_ms += (u32)msec;

	return time_ms;
}



GF_EXPORT
s32 gf_net_get_timezone()
{
#if defined(_WIN32_WCE)
	return 0;
#else
	//this has been commented due to some reports of broken implementation on some systems ...
	//		s32 val = timezone;
	//		return val;


	/*FIXME - avoid errors at midnight when estimating timezone this does not work !!*/
	s32 t_timezone;
	struct tm t_gmt, t_local;
	time_t t_time;
	t_time = time(NULL);
	t_gmt = *gf_gmtime(&t_time);
	t_local = *localtime(&t_time);

	t_timezone = (t_gmt.tm_hour - t_local.tm_hour) * 3600 + (t_gmt.tm_min - t_local.tm_min) * 60;
	return t_timezone;
#endif
}

GF_EXPORT
Bool gf_net_time_is_dst()
{
#if defined(_WIN32_WCE)
	return GF_FALSE;
#else
	struct tm t_local;
	time_t t_time;
	t_time = time(NULL);
	t_local = *localtime(&t_time);
	return t_local.tm_isdst ? GF_TRUE : GF_FALSE;
#endif
}
//no mkgmtime on mingw..., use our own
#if (defined(WIN32) && defined(__GNUC__))

static Bool leap_year(u32 year) {
	year += 1900;
	return (year % 4) == 0 && ((year % 100) != 0 || (year % 400) == 0) ? GF_TRUE : GF_FALSE;
}
static time_t gf_mktime_utc(struct tm *tm)
{
	static const u32 days_per_month[2][12] = {
		{31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
		{31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
	};
	time_t time=0;
	int i;

	for (i=70; i<tm->tm_year; i++) {
		time += leap_year(i) ? 366 : 365;
	}

	for (i=0; i<tm->tm_mon; ++i) {
		time += days_per_month[leap_year(tm->tm_year)][i];
	}
	time += tm->tm_mday - 1;
	time *= 24;
	time += tm->tm_hour;
	time *= 60;
	time += tm->tm_min;
	time *= 60;
	time += tm->tm_sec;
	return time;
}

#elif defined(WIN32)
static time_t gf_mktime_utc(struct tm *tm)
{
	return  _mkgmtime(tm);
}

#elif defined(GPAC_CONFIG_ANDROID)
#if defined(__LP64__)
static time_t gf_mktime_utc(struct tm *tm)
{
	return timegm(tm);
}
#else
#include <time64.h>
static time_t gf_mktime_utc(struct tm *tm)
{
	static const time_t kTimeMax = ~(1L << (sizeof(time_t) * CHAR_BIT - 1));
	static const time_t kTimeMin = (1L << (sizeof(time_t) * CHAR_BIT - 1));
	time64_t result = timegm64(tm);
	if (result < kTimeMin || result > kTimeMax)
		return -1;
	return result;
}
#endif

#else

static time_t gf_mktime_utc(struct tm *tm)
{
	return timegm(tm);
}

#endif

GF_EXPORT
u64 gf_net_parse_date(const char *val)
{
	u64 current_time;
	char szDay[50], szMonth[50];
	u32 year, month, day, h, m, s, ms;
	s32 oh, om;
	Float secs;
	Bool neg_time_zone = GF_FALSE;

#ifdef _WIN32_WCE
	SYSTEMTIME syst;
	FILETIME filet;
#else
	struct tm t;
	memset(&t, 0, sizeof(struct tm));
#endif

	szDay[0] = szMonth[0] = 0;
	year = month = day = h = m = s = 0;
	oh = om = 0;
	secs = 0;

	if (sscanf(val, "%d-%d-%dT%d:%d:%g-%d:%d", &year, &month, &day, &h, &m, &secs, &oh, &om) == 8) {
		neg_time_zone = GF_TRUE;
	}
	else if (sscanf(val, "%d-%d-%dT%d:%d:%g+%d:%d", &year, &month, &day, &h, &m, &secs, &oh, &om) == 8) {
	}
	else if (sscanf(val, "%d-%d-%dT%d:%d:%gZ", &year, &month, &day, &h, &m, &secs) == 6) {
	}
	else if (sscanf(val, "%3s, %d %3s %d %d:%d:%d", szDay, &day, szMonth, &year, &h, &m, &s)==7) {
		secs  = (Float) s;
	}
	else if (sscanf(val, "%9s, %d-%3s-%d %02d:%02d:%02d GMT", szDay, &day, szMonth, &year, &h, &m, &s)==7) {
		secs  = (Float) s;
	}
	else if (sscanf(val, "%3s %3s %d %02d:%02d:%02d %d", szDay, szMonth, &day, &year, &h, &m, &s)==7) {
		secs  = (Float) s;
	}
	else if (sscanf(val, LLU, &current_time) == 1 && current_time > 1000000000 && current_time < GF_INT_MAX) {
		return current_time * 1000; // guessed raw duration since UTC0 in seconds
	}
	else if (sscanf(val, LLU, &current_time) == 1 && current_time > 1000000000000ULL && current_time < GF_INT_MAX * 1000ULL) {
		return current_time; // guessed duration since UTC0 in milliseconds
	} else {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot parse date string %s\n", val));
		return 0;
	}

	if (month) {
		month -= 1;
	} else {
		if (!strcmp(szMonth, "Jan")) month = 0;
		else if (!strcmp(szMonth, "Feb")) month = 1;
		else if (!strcmp(szMonth, "Mar")) month = 2;
		else if (!strcmp(szMonth, "Apr")) month = 3;
		else if (!strcmp(szMonth, "May")) month = 4;
		else if (!strcmp(szMonth, "Jun")) month = 5;
		else if (!strcmp(szMonth, "Jul")) month = 6;
		else if (!strcmp(szMonth, "Aug")) month = 7;
		else if (!strcmp(szMonth, "Sep")) month = 8;
		else if (!strcmp(szMonth, "Oct")) month = 9;
		else if (!strcmp(szMonth, "Nov")) month = 10;
		else if (!strcmp(szMonth, "Dec")) month = 11;
	}

#ifdef _WIN32_WCE
	memset(&syst, 0, sizeof(SYSTEMTIME));
	syst.wYear = year;
	syst.wMonth = month + 1;
	syst.wDay = day;
	syst.wHour = h;
	syst.wMinute = m;
	syst.wSecond = (u32) secs;
	SystemTimeToFileTime(&syst, &filet);
	current_time = (u64) ((*(LONGLONG *) &filet - TIMESPEC_TO_FILETIME_OFFSET) / 10000000);

#else

	t.tm_year = year>1000 ? year-1900 : year;
	t.tm_mday = day;
	t.tm_hour = h;
	t.tm_min = m;
	t.tm_sec = (u32) secs;
	t.tm_mon = month;

	if (strlen(szDay) ) {
		if (!strcmp(szDay, "Mon") || !strcmp(szDay, "Monday")) t.tm_wday = 0;
		else if (!strcmp(szDay, "Tue") || !strcmp(szDay, "Tuesday")) t.tm_wday = 1;
		else if (!strcmp(szDay, "Wed") || !strcmp(szDay, "Wednesday")) t.tm_wday = 2;
		else if (!strcmp(szDay, "Thu") || !strcmp(szDay, "Thursday")) t.tm_wday = 3;
		else if (!strcmp(szDay, "Fri") || !strcmp(szDay, "Friday")) t.tm_wday = 4;
		else if (!strcmp(szDay, "Sat") || !strcmp(szDay, "Saturday")) t.tm_wday = 5;
		else if (!strcmp(szDay, "Sun") || !strcmp(szDay, "Sunday")) t.tm_wday = 6;
	}

	current_time = gf_mktime_utc(&t);

	if ((s64) current_time == -1) {
		//use 1 ms
		return 1;
	}
	if (current_time == 0 && secs == 0.0) {
		//use 1 ms
		return 1;
	}

#endif

	if (om || oh) {
		s32 diff = (60*oh + om)*60;
		if (!neg_time_zone) diff = -diff;
		current_time = current_time + diff;
	}
	current_time *= 1000;
	ms = (u32) ( (secs - (u32) secs) * 1000);
	return current_time + ms;
}

GF_EXPORT
u64 gf_net_get_utc_ts(u32 year, u32 month, u32 day, u32 hour, u32 min, u32 sec)
{
	u64 current_time;
#ifdef _WIN32_WCE
	SYSTEMTIME syst;
	FILETIME filet;
#else
	struct tm t;
	memset(&t, 0, sizeof(struct tm));
#endif

#ifdef _WIN32_WCE
	memset(&syst, 0, sizeof(SYSTEMTIME));
	syst.wYear = year;
	syst.wMonth = month + 1;
	syst.wDay = day;
	syst.wHour = hour;
	syst.wMinute = min;
	syst.wSecond = (u32) sec;
	SystemTimeToFileTime(&syst, &filet);
	current_time = (u64) ((*(LONGLONG *) &filet - TIMESPEC_TO_FILETIME_OFFSET) / 10000000);
#else
	t.tm_year = year>1000 ? year-1900 : year;
	t.tm_mday = day;
	t.tm_hour = hour;
	t.tm_min = min;
	t.tm_sec = (u32) sec;
	t.tm_mon = month;

	current_time = gf_mktime_utc(&t);
	if ((s64) current_time == -1) {
		//use 1 ms
		return 1;
	}
	if (current_time == 0) {
		//use 1 ms
		return 1;
	}
#endif

	current_time *= 1000;
	return current_time;
}

GF_EXPORT
u64 gf_net_ntp_to_utc(u64 ntp)
{
	u64 current_time;
	Double msec;
	u32 sec = ntp>>32;
	u32 frac = ntp & 0xFFFFFFFFUL;

	current_time = sec - GF_NTP_SEC_1900_TO_1970;
	current_time *= 1000;
	msec = frac*1000.0;
	msec /= 0xFFFFFFFF;
	current_time += (u64) msec;
	return current_time;
}

GF_EXPORT
u64 gf_net_get_utc()
{
	u64 current_time;
	Double msec;
	u32 sec, frac;

	gf_net_get_ntp(&sec, &frac);
	current_time = sec - GF_NTP_SEC_1900_TO_1970;
	current_time *= 1000;
	msec = frac*1000.0;
	msec /= 0xFFFFFFFF;
	current_time += (u64) msec;
	return current_time;
}



GF_EXPORT
GF_Err gf_bin128_parse(const char *string, bin128 value)
{
	u32 len;
	u32	i=0;
	if (!strnicmp(string, "0x", 2)) string += 2;
	len = (u32) strlen(string);
	if (len >= 32) {
		u32 j;
		for (j=0; j<len; j+=2) {
			u32 v;
			char szV[5];

			while (string[j] && !isalnum(string[j]))
				j++;
			if (!string[j])
				break;
			sprintf(szV, "%c%c", string[j], string[j+1]);
			sscanf(szV, "%x", &v);
			if (i > 15) {
				// force error check below
				i++;
				break;
			}
			value[i] = v;
			i++;

		}
	}
	if (i != 16) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CORE] 128bit blob is not 16-bytes long: %s\n", string));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}


GF_EXPORT
GF_Err gf_file_load_data_filep(FILE *file, u8 **out_data, u32 *out_size)
{
	u64 fsize;
	*out_data = NULL;
	*out_size = 0;

	fsize = gf_fsize(file);
	if (fsize>0xFFFFFFFFUL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ( "[Core] file %s is too big to load in memory (" LLU " bytes)\n", fsize) );
		return GF_OUT_OF_MEM;
	}

	*out_size = (u32) fsize;
	if (fsize == 0) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] file is empty\n"));
		return GF_OK;
	}

	/* First, read the dump in a buffer */
	*out_data = gf_malloc((size_t)(fsize+1) * sizeof(char));
	if (! *out_data) {
		return GF_OUT_OF_MEM;
	}
	fsize = gf_fread(*out_data, (size_t)fsize, file);

	if ((u32) fsize != *out_size) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] file read failed\n"));
		gf_free(*out_data);
		*out_data = NULL;
		*out_size = 0;
		return GF_IO_ERR;
	}
	(*out_data)[fsize] = 0;
	return GF_OK;
}

GF_EXPORT
GF_Err gf_file_load_data(const char *file_name, u8 **out_data, u32 *out_size)
{
	GF_Err e;
	FILE *file = gf_fopen(file_name, "rb");

	if (!file) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot open file %s\n", file_name));
		return GF_IO_ERR;
	}
	e = gf_file_load_data_filep(file, out_data, out_size);
	gf_fclose(file);
	return e;
}

#ifndef WIN32
#include <unistd.h>
GF_EXPORT
u32 gf_sys_get_process_id()
{
	return getpid ();
}
#else
#include <windows.h>
GF_EXPORT
u32 gf_sys_get_process_id()
{
	return GetCurrentProcessId();
}
#endif

#ifndef WIN32
#include <termios.h>
GF_EXPORT
int gf_getch() {
	struct termios old;
	struct termios new;
	int rc;
	if (tcgetattr(0, &old) == -1) {
		return -1;
	}
	new = old;
	new.c_lflag &= ~(ICANON | ECHO | ISIG);
	new.c_cc[VMIN] = 1;
	new.c_cc[VTIME] = 0;
	if (tcsetattr(0, TCSANOW, &new) == -1) {
		return -1;
	}
	rc = getchar();
	(void) tcsetattr(0, TCSANOW, &old);
	return rc;
}
#else
GF_EXPORT
int gf_getch() {
	return getchar();
}
#endif

GF_EXPORT
Bool gf_read_line_input(char * line, int maxSize, Bool showContent) {
	char read;
	int i = 0;
	if (fflush( stderr ))
		perror("Failed to flush buffer %s");
	do {
		line[i] = '\0';
		if (i >= maxSize - 1)
			return GF_FALSE;
		read = gf_getch();
		if (read == 8 || read == 127) {
			if (i > 0) {
				fprintf(stderr, "\b \b");
				i--;
			}
		} else if (read >= 32) {
			fputc(showContent ? read : '*', stderr);
			line[i++] = read;
		}
		fflush(stderr);
	} while (read != '\n');
	if (!read)
		return GF_FALSE;
	return GF_TRUE;
}


GF_EXPORT
GF_Err gf_creds_check_password(const char *user, char *password)
{
	u32 i, len, v;
	u8 passbuf[117];
	u8 hash[20], ohash[20];
	const char *cred_file = gf_opts_get_key("core", "users");
	if (!cred_file) return GF_NOT_FOUND;
	GF_Config *creds = gf_cfg_new(NULL, cred_file);
	if (!creds) return GF_NOT_FOUND;
	len = strlen(password);
	const char *pass_hex = gf_cfg_get_key(creds, user, "password");
	const char *salt_hex = gf_cfg_get_key(creds, user, "salt");
	if (!salt_hex || !pass_hex || (strlen(salt_hex) != 32) || (strlen(pass_hex) != 40) || (len>=100)) {
		gf_cfg_del(creds);
		return GF_NOT_FOUND;
	}

	u8 salt[16];
	for (i=0; i<16; i++) {
		sscanf(salt_hex+2*i, "%02x", &v);
		salt[i] = v;
	}
	for (i=0; i<20; i++) {
		sscanf(pass_hex+2*i, "%02x", &v);
		ohash[i] = v;
	}

	memcpy(passbuf, password, len);
	passbuf[len]='@';
	memcpy(passbuf+len+1, salt, 16);
	len+=17;
	gf_sha1_csum(passbuf, len, hash);
	len=1;
	for (i=0; i<20; i++) {
		if (hash[i] != ohash[i]) len=0;
	}

	if (len) {
		sprintf((char *)passbuf, LLU, gf_net_get_utc());
		gf_cfg_set_key(creds, user, "last_auth", (char *)passbuf);
		gf_cfg_save(creds);
	}
	gf_cfg_del(creds);

	if (!len) return GF_AUTHENTICATION_FAILURE;
	return GF_OK;
}

#include <gpac/token.h>

GF_EXPORT
Bool gf_creds_check_membership(const char *username, const char *users, const char *groups)
{
	if (!username) return GF_FALSE;
	//users specified and username is listed
	if (users && gf_token_find_word(users, username, ",")) return GF_TRUE;

	if (!groups)
		return users ? GF_TRUE : GF_FALSE;

	//check all defined groups
	const char *cred_file = gf_opts_get_key("core", "users");
	if (!cred_file) return GF_FALSE;
	GF_Config *creds = gf_cfg_new(NULL, cred_file);
	if (!creds) return GF_FALSE;
	u32 i, nb_groups = gf_cfg_get_key_count(creds, "groups");
	for (i=0; i<nb_groups; i++) {
		const char *g = gf_cfg_get_key_name(creds, "groups", i);
		if (!gf_token_find_word(groups, g, ",")) continue;
		//group is present in groups being checked, check user
		const char *members = gf_cfg_get_key(creds, "groups", g);
		if (gf_token_find_word(members, username, ",")) {
			gf_cfg_del(creds);
			return GF_TRUE;
		}
	}
	gf_cfg_del(creds);
	return GF_FALSE;
}
