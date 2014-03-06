/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2012
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


#ifndef WIN32

GF_EXPORT
u32 gf_sys_clock()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return ( (now.tv_sec)*1000 + (now.tv_usec) / 1000) - sys_start_time;
}

GF_EXPORT
u64 gf_sys_clock_high_res()
{
	struct timeval now;
	gettimeofday(&now, NULL);
	return ( (now.tv_sec)*1000000 + (now.tv_usec) - sys_start_time_hr;
}

#endif



GF_EXPORT
void gf_sleep(u32 ms)
{
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
}


GF_Err gf_rmdir(char *DirPathName)
{
#if defined (_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	BOOL res;
	CE_CharToWide(DirPathName, swzName);
	res = RemoveDirectory(swzName);
	if (! res) {
		int err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot delete director %s: last error %d\n", DirPathName, err ));
	}
#elif defined (WIN32)
	int res = rmdir(DirPathName);
	if (res==-1) {
		int err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot delete director %s: last error %d\n", DirPathName, err ));
		return GF_IO_ERR;
	}
#else
    int res = rmdir(DirPathName);
	if (res==-1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot delete director %s: last error %d\n", DirPathName, errno  ));
		return GF_IO_ERR;
	}
#endif
	return GF_OK;
}   

GF_EXPORT
GF_Err gf_mkdir(char* DirPathName)
{
#if defined (_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	BOOL res;
	CE_CharToWide(DirPathName, swzName);
	res = CreateDirectory(swzName, NULL);
	if (! res) {
		int err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create directory %s: last error %d\n", DirPathName, err ));
	}
#elif defined (WIN32) 
	int res = mkdir(DirPathName);
	if (res==-1) {
		int err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create directory %s: last error %d\n", DirPathName, err ));
	}
#else	
    int res = mkdir(DirPathName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (res==-1) {		
		if(errno == 17){
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create directory %s, it already exists: last error %d \n", DirPathName, errno ));
			return GF_BAD_PARAM;
		}else{
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create directory %s: last error %d\n", DirPathName, errno ));
			return GF_IO_ERR;
		}
	}
#endif
	return GF_OK;
}

static Bool delete_dir(void *cbck, char *item_name, char *item_path)
{
	Bool directory_clean_mode = *(Bool*)cbck;

	if(directory_clean_mode){
		 gf_cleanup_dir(item_path);
		 gf_rmdir(item_path);		
	}else{
		gf_delete_file(item_path);		
	}	
	return 0;
}

GF_Err gf_cleanup_dir(char* DirPathName)
{
	Bool directory_clean_mode;

	directory_clean_mode = 1;
	gf_enum_directory(DirPathName, 1, delete_dir, &directory_clean_mode, NULL);														
	directory_clean_mode = 0;
	gf_enum_directory(DirPathName, 0, delete_dir, &directory_clean_mode, NULL);

	return GF_OK;
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
    static int      tzflag;

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

s32 gettimeofday(struct timeval *tp, void *tz)
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
GF_Err gf_delete_file(const char *fileName)
{
#if defined(_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	CE_CharToWide((char*)fileName, swzName);
	return (DeleteFile(swzName)==0) ? GF_IO_ERR : GF_OK;
#elif defined(WIN32)
	/* success if != 0 */
	return (DeleteFile(fileName)==0) ? GF_IO_ERR : GF_OK;
#else
	/* success is == 0 */
	return ( remove(fileName) == 0) ? GF_OK : GF_IO_ERR;
#endif
}

GF_EXPORT
GF_Err gf_move_file(const char *fileName, const char *newFileName)
{
#if defined(_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	TCHAR swzNewName[MAX_PATH];
	CE_CharToWide((char*)fileName, swzName);
	CE_CharToWide((char*)newFileName, swzNewName);
	return (MoveFile(swzName, swzNewName) == 0 ) ? GF_IO_ERR : GF_OK;
#elif defined(WIN32)
	/* success if != 0 */
	return (MoveFile(fileName, newFileName) == 0 ) ? GF_IO_ERR : GF_OK;
#else
	/* success is == 0 */
	char cmd[1024];
	if (!fileName || !newFileName)
		return GF_IO_ERR;
	snprintf(cmd, sizeof(cmd), "mv '%s' '%s' > /dev/null 2>&1", fileName, newFileName);
	return ( system(cmd) == 0) ? GF_OK : GF_IO_ERR;
#endif
}

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
u64 gf_file_modification_time(const char *filename)
{
#if defined(_WIN32_WCE) 
	WCHAR _file[GF_MAX_PATH]; 
	WIN32_FIND_DATA FindData;
	HANDLE fh;
	ULARGE_INTEGER uli;
	ULONGLONG time_ms;
	BOOL ret;
	CE_CharToWide((char *) filename, _file);
	fh = FindFirstFile(_file, &FindData);
	if (fh == INVALID_HANDLE_VALUE) return 0;
	uli.LowPart = FindData.ftLastWriteTime.dwLowDateTime;
	uli.HighPart = FindData.ftLastWriteTime.dwHighDateTime;
	ret = FindClose(fh);
	if (!ret) {
		DWORD err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] FindClose() in gf_file_modification_time() returned the following error code: %d\n", err));
	}
	time_ms = uli.QuadPart/10000;
	return time_ms;
#elif defined(WIN32) && !defined(__GNUC__)
	struct _stat64 sb;
	if (_stat64(filename, &sb) != 0) return 0;
	return sb.st_mtime;
#else
	struct stat sb;
	if (stat(filename, &sb) != 0) return 0;
	return sb.st_mtime;
#endif
	return 0;
}

GF_EXPORT
FILE *gf_temp_file_new()
{
#if defined(_WIN32_WCE)
	TCHAR pPath[MAX_PATH+1];
	TCHAR pTemp[MAX_PATH+1];
	if (!GetTempPath(MAX_PATH, pPath)) {
		pPath[0] = '.';
		pPath[1] = '.';
	}
	if (GetTempFileName(pPath, TEXT("git"), 0, pTemp))
		return _wfopen(pTemp, TEXT("w+b"));

	return NULL;
#elif defined(WIN32)
	char tmp[MAX_PATH], t_file[100];
	FILE *res = tmpfile();
	if (res) return res;
	{
		u32 err = GetLastError();
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Win32] system failure for tmpfile(): 0x%08x\n", err));
	}
	/*tmpfile() may fail under vista ...*/
	if (!GetEnvironmentVariable("TEMP",tmp,MAX_PATH)) 
		return NULL;
	sprintf(t_file, "\\gpac_%08x.tmp", (u32) tmp);
	strcat(tmp, t_file);
	return gf_f64_open(tmp, "w+b");
#else
	return tmpfile(); 
#endif
}

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
	*sec = tv.tv_sec;
	*msec = tv.tv_usec/1000;
#endif
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
#if 0
	struct passwd *pw;
	pw = getpwuid(getuid());
	strcpy(buf, "");
	if (pw && pw->pw_name) strcpy(name, pw->pw_name);
#endif
}


/*enumerate directories*/
GF_EXPORT
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, gf_enum_dir_item enum_dir_fct, void *cbck, const char *filter)
{
	char item_path[GF_MAX_PATH];

#if defined(_WIN32_WCE)
	char _path[GF_MAX_PATH];
	unsigned short path[GF_MAX_PATH];
	unsigned short w_filter[GF_MAX_PATH];
	char file[GF_MAX_PATH];
#else
	char path[GF_MAX_PATH], *file;
#endif

#ifdef WIN32
	WIN32_FIND_DATA FindData;
	HANDLE SearchH;
#else	
	DIR *the_dir;
	struct dirent* the_file;
	struct stat st;
#endif

	if (!dir || !enum_dir_fct) return GF_BAD_PARAM;

	if (filter && (!strcmp(filter, "*") || !filter[0])) filter=NULL;

	if (!strcmp(dir, "/")) {
#if defined(WIN32) && !defined(_WIN32_WCE)
		u32 len;
		char *drives, *volume;
		len = GetLogicalDriveStrings(0, NULL);
		drives = gf_malloc(sizeof(char)*(len+1));
		drives[0]=0;
		GetLogicalDriveStrings(len, drives);
		len = (u32) strlen(drives);
		volume = drives;
		while (len) {
			enum_dir_fct(cbck, volume, "");
			volume += len+1;
			len = (u32) strlen(volume);
		}
		gf_free(drives);
		return GF_OK;
#elif defined(__SYMBIAN32__)
		RFs iFs;
		TDriveList aList;
		iFs.Connect();
		iFs.DriveList(aList);
		for (TInt i=0;i<KMaxDrives;i++) {
			if (aList[i]) {
				char szDrive[10];
				TChar aDrive;
				iFs.DriveToChar(i, aDrive);
				sprintf(szDrive, "%c:", (TUint)aDrive);
				enum_dir_fct(cbck, szDrive, "");
			}
		}
		iFs.Close();
		FlushItemList();
		return GF_OK;
#endif
	}


#if defined (_WIN32_WCE)
	switch (dir[strlen(dir) - 1]) {
	case '/':
	case '\\':
		sprintf(_path, "%s*", dir);
		break;
	default:
		sprintf(_path, "%s%c*", dir, GF_PATH_SEPARATOR);
		break;
	}
	CE_CharToWide(_path, path);
	CE_CharToWide((char *)filter, w_filter);
#elif defined(WIN32)
	switch (dir[strlen(dir) - 1]) {
	case '/':
	case '\\':
		sprintf(path, "%s*", dir);
		break;
	default:
		sprintf(path, "%s%c*", dir, GF_PATH_SEPARATOR);
		break;
	}
#else
	strcpy(path, dir);
	if (path[strlen(path)-1] != '/') strcat(path, "/");
#endif

#ifdef WIN32
	SearchH= FindFirstFile(path, &FindData);
	if (SearchH == INVALID_HANDLE_VALUE) return GF_IO_ERR;

#if defined (_WIN32_WCE)
	_path[strlen(_path)-1] = 0;
#else
	path[strlen(path)-1] = 0;
#endif

	while (SearchH != INVALID_HANDLE_VALUE) {

#else

	the_dir = opendir(path);
	if (the_dir == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot open directory %s for enumeration\n", path));
		return GF_IO_ERR;
	}
	the_file = readdir(the_dir);
	while (the_file) {

#endif

#if defined (_WIN32_WCE)
		if (!wcscmp(FindData.cFileName, _T(".") )) goto next;
		if (!wcscmp(FindData.cFileName, _T("..") )) goto next;
#elif defined(WIN32)
		if (!strcmp(FindData.cFileName, ".")) goto next;
		if (!strcmp(FindData.cFileName, "..")) goto next;
#else
		if (!strcmp(the_file->d_name, "..")) goto next;
		if (the_file->d_name[0] == '.') goto next;
#endif

#ifdef WIN32
		if (!enum_directory && (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) goto next;
		if (enum_directory && !(FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) goto next;
#endif

		if (filter) {
#if defined (_WIN32_WCE)
			short ext[30];
			short *sep = wcsrchr(FindData.cFileName, (wchar_t) '.');
			if (!sep) goto next;
			wcscpy(ext, sep+1);
			wcslwr(ext);
			if (!wcsstr(w_filter, ext)) goto next;
#elif defined(WIN32)
			char ext[30];
			char *sep = strrchr(FindData.cFileName, '.');
			if (!sep) goto next;
			strcpy(ext, sep+1);
			strlwr(ext);
			if (!strstr(filter, ext)) goto next;
#else
			char ext[30];
			char *sep = strrchr(the_file->d_name, '.');
			if (!sep) goto next;
			strcpy(ext, sep+1);
			strlwr(ext);
			if (!strstr(filter, sep+1)) goto next;
#endif
		}

#if defined (_WIN32_WCE)
		CE_WideToChar(FindData.cFileName, file);
		strcpy(item_path, _path);
		strcat(item_path, file);
#elif defined(WIN32)
		strcpy(item_path, path);
		strcat(item_path, FindData.cFileName);
		file = FindData.cFileName;
#else
		strcpy(item_path, path);
		strcat(item_path, the_file->d_name);
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Checking file %s for enum\n", item_path));
		
		if (stat( item_path, &st ) != 0) goto next;
		if (enum_directory && ( (st.st_mode & S_IFMT) != S_IFDIR)) goto next;
		if (!enum_directory && ((st.st_mode & S_IFMT) == S_IFDIR)) goto next;
		file = the_file->d_name;
#endif
		if (enum_dir_fct(cbck, file, item_path)) {
#ifdef WIN32
			BOOL ret = FindClose(SearchH);
			if (!ret) {
				DWORD err = GetLastError();
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] FindClose() in gf_enum_directory() returned(1) the following error code: %d\n", err));
			}
#endif
			break;
		}

next:
#ifdef WIN32
		if (!FindNextFile(SearchH, &FindData)) {
			BOOL ret = FindClose(SearchH);
			if (!ret) {
				DWORD err = GetLastError();
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] FindClose() in gf_enum_directory() returned(2) the following error code: %d\n", err));
			}
			break;
		}
#else
		the_file = readdir(the_dir);
#endif
	}
#ifndef WIN32
	closedir(the_dir);
#endif
	return GF_OK;
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

GF_EXPORT
u64 gf_f64_tell(FILE *fp)
{
#if defined(_WIN32_WCE)
	return (u64) ftell(fp);
#elif defined(GPAC_CONFIG_WIN32)	/* mingw or cygwin */
#if (_FILE_OFFSET_BITS >= 64)
	return (u64) ftello64(fp);
#else
	return (u64) ftell(fp);
#endif
#elif defined(WIN32)
	return (u64) _ftelli64(fp);
#elif defined(GPAC_CONFIG_LINUX) && !defined(GPAC_ANDROID)
	return (u64) ftello64(fp);
#elif (defined(GPAC_CONFIG_FREEBSD) || defined(GPAC_CONFIG_DARWIN))
	return (u64) ftello(fp);
#else
	return (u64) ftell(fp);
#endif
}

GF_EXPORT
u64 gf_f64_seek(FILE *fp, s64 offset, s32 whence)
{
#if defined(_WIN32_WCE)
	return (u64) fseek(fp, (s32) offset, whence);
#elif defined(GPAC_CONFIG_WIN32)	/* mingw or cygwin */
#if (_FILE_OFFSET_BITS >= 64)
	return (u64) fseeko64(fp, offset, whence);
#else
	return (u64) fseek(fp, (s32) offset, whence);
#endif
#elif defined(WIN32)
	return (u64) _fseeki64(fp, offset, whence);
#elif defined(GPAC_CONFIG_LINUX) && !defined(GPAC_ANDROID)
	return fseeko64(fp, (off64_t) offset, whence);
#elif (defined(GPAC_CONFIG_FREEBSD) || defined(GPAC_CONFIG_DARWIN))
	return fseeko(fp, (off_t) offset, whence);
#else
	return fseek(fp, (s32) offset, whence);
#endif
}

GF_EXPORT
FILE *gf_f64_open(const char *file_name, const char *mode)
{
#if defined(WIN32)
	FILE *res = fopen(file_name, mode);
	if (res) return res;
	if (strchr(mode, 'w') || strchr(mode, 'a')) {
		u32 err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Win32] system failure for file opening of %s in mode %s: 0x%08x\n", file_name, mode, err));
	}
	return NULL;
#elif defined(GPAC_CONFIG_LINUX) && !defined(GPAC_ANDROID)
	return fopen64(file_name, mode);
#elif (defined(GPAC_CONFIG_FREEBSD) || defined(GPAC_CONFIG_DARWIN))
	return fopen(file_name, mode);
#else
	return fopen(file_name, mode);
#endif
}

#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! defined(_GNU_SOURCE)
#define HAVE_STRERROR_R 1
#endif

GF_EXPORT
size_t gf_fwrite(const void *ptr, size_t size, size_t nmemb, 
                       FILE *stream)
{
	size_t result = fwrite(ptr, size, nmemb, stream);
	if (result != nmemb) {
#ifdef _WIN32_WCE
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Error writing data: %d blocks to write but %d blocks written\n", nmemb, result));
#else
#if defined WIN32 && !defined(GPAC_CONFIG_WIN32)
		errno_t errno_save;
		_get_errno(&errno_save);
#else
		int errno_save = errno;
#endif
		//if (errno_save!=0)
		{
#ifdef HAVE_STRERROR_R
#define ERRSTR_BUF_SIZE 256
			char errstr[ERRSTR_BUF_SIZE];
			if(strerror_r(errno_save, errstr, ERRSTR_BUF_SIZE) != 0)
			{
				strerror_r(0, errstr, ERRSTR_BUF_SIZE);
			}
#else
			char *errstr = (char*)strerror(errno_save);
#endif
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Error writing data (%s): %d blocks to write but %d blocks written\n", errstr, nmemb, result));
		}
#endif
	}
	return result;
}


/*seems OK under mingw also*/
#ifdef WIN32
#ifdef _WIN32_WCE

Bool gf_prompt_has_input()
{
	return 0;
}
char gf_prompt_get_char() { return 0; }
void gf_prompt_set_echo_off(Bool echo_off) { return; }

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

void gf_prompt_set_echo_off(Bool echo_off) 
{
	DWORD flags;
	HANDLE hStdin = GetStdHandle(STD_INPUT_HANDLE);
	BOOL ret = GetConsoleMode(hStdin, &flags);
	if (!ret) {
		DWORD err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CONSOLE, ("[Console] GetConsoleMode() return with the following error code: %d\n", err));
	}
	if (echo_off) flags &= ~ENABLE_ECHO_INPUT;
	else flags |= ENABLE_ECHO_INPUT;
	SetConsoleMode(hStdin, flags);
}
#endif
#else
/*linux kbhit/getchar- borrowed on debian mailing lists, (author Mike Brownlow)*/
#include <termios.h>

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

void gf_prompt_set_echo_off(Bool echo_off) 
{ 
	init_keyboard();
	if (echo_off) t_orig.c_lflag &= ~ECHO;
	else t_orig.c_lflag |= ECHO;
	close_keyboard(0);
}

GF_EXPORT
Bool gf_prompt_has_input()
{
	u8 ch;
	s32 nread;

	init_keyboard();
	if (ch_peek != -1) return 1;
	t_new.c_cc[VMIN]=0;
	tcsetattr(0, TCSANOW, &t_new);
	nread = read(0, &ch, 1);
	t_new.c_cc[VMIN]=1;
	tcsetattr(0, TCSANOW, &t_new);
	if(nread == 1) {
		ch_peek = ch;
		return 1;
	}
	close_keyboard(0);
	return 0;
}

GF_EXPORT
char gf_prompt_get_char()
{
	char ch;
	if (ch_peek != -1) {
		ch = ch_peek;
		ch_peek = -1;
		close_keyboard(1);
		return ch;
	}
	if (0==read(0,&ch,1))
		ch = 0;
	close_keyboard(1);
	return ch;
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

#ifdef GPAC_MEMORY_TRACKING
void gf_mem_enable_tracker();
#endif

static u64 memory_at_gpac_startup = 0;

GF_EXPORT
void gf_sys_init(Bool enable_memory_tracker)
{
	if (!sys_init) {
#if defined (WIN32)
#if defined(_WIN32_WCE)
		MEMORYSTATUS ms;
#else
		SYSTEM_INFO sysinfo;
#endif
#endif

		if (enable_memory_tracker) {
#ifdef GPAC_MEMORY_TRACKING
			gf_mem_enable_tracker();
#endif
		}
		/*by default log subsystem is initialized to error on all tools, and info on console to debug scripts*/
		gf_log_set_tool_level(GF_LOG_ALL, GF_LOG_ERROR);
		gf_log_set_tool_level(GF_LOG_CONSOLE, GF_LOG_INFO);


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

#ifdef GPAC_CONFIG_FREEBSD
		{
			s32 flags[4];
			size_t len = sizeof(u32); 
			flags[0] = CTL_HW;
			flags[1] = HW_AVAILCPU;
			sysctl(flags, 2, &the_rti.nb_cores, &len, NULL, 0);
			if( the_rti.nb_cores < 1 ) {
				flags[1] = HW_NCPU;
				sysctl(flags, 2, &the_rti.nb_cores, &len, NULL, 0);
				if (the_rti.nb_cores<1) the_rti.nb_cores = 1;
			}
		}
#else
		the_rti.nb_cores = sysconf( _SC_NPROCESSORS_ONLN );
#endif

		sys_start_time = gf_sys_clock();
		sys_start_time_hr = gf_sys_clock_high_res();
#endif
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[core] process id %d\n", the_rti.pid));

#ifndef _WIN32_WCE
		setlocale( LC_NUMERIC, "C" ); 
#endif
	}
	sys_init += 1;


	/*init RTI stats*/
	if (!memory_at_gpac_startup) {
		GF_SystemRTInfo rti;
		gf_sys_get_rti(500, &rti, GF_RTI_SYSTEM_MEMORY_ONLY);
		memory_at_gpac_startup = rti.physical_memory_avail;
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[core] System init OK - process id %d - %d MB physical RAM - %d cores\n", rti.pid, (u32) (rti.physical_memory/1024/1024), rti.nb_cores));
	}
}

GF_EXPORT
void gf_sys_close()
{
	if (sys_init > 0) {
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

#ifdef GPAC_MEMORY_TRACKING
		gf_memory_print();
#endif
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
	u32 nb_threads, entry_time;
	HANDLE hSnapShot;

	assert(sys_init);

	if (!rti) return 0;

	proc_idle_time = proc_k_u_time = process_k_u_time = 0;
	nb_threads = 0;

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
#ifdef GPAC_MEMORY_TRACKING
		rti->gpac_memory = (u64) gpac_allocated_memory;
#endif
		return 1;
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
		if (!hSnapShot) return 0;
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
						nb_threads = pentry.cntThreads;
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
		if (!process_k_u_time) return 0;
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
		the_rti.total_cpu_usage = (u32) (100 * the_rti.total_cpu_time_diff / the_rti.sampling_period_duration);
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
	return 1;
}

	
#elif defined(GPAC_CONFIG_DARWIN) && !defined(GPAC_IPHONE)

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
			mach_error("[RTI] Unexpected thread_info() call return", error);
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[RTI] Unexpected thread info for PID %d\n", the_rti.pid));
			break;
		}
		if ((thi->flags & TH_FLAGS_IDLE) == 0) {
			utime += thi->user_time.seconds + thi->user_time.microseconds * 1e-6;
			stime += thi->system_time.seconds + thi->system_time.microseconds * 1e-6;
			percent +=  (u32) (100 * (double)thi->cpu_usage / TH_USAGE_SCALE);
		}
	}
	error = vm_deallocate(mach_task_self(), (vm_offset_t)thread_table, table_size * sizeof(thread_array_t));
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

//linux
#else

Bool gf_sys_get_rti_os(u32 refresh_time_ms, GF_SystemRTInfo *rti, u32 flags)
{
  u32 entry_time;
  u64 process_u_k_time;
  u32 u_k_time, idle_time;
#if 0
  char szProc[100];
#endif
  char line[2048];
  FILE *f;

  assert(sys_init);

  entry_time = gf_sys_clock();
  if (last_update_time && (entry_time - last_update_time < refresh_time_ms)) {
	  memcpy(rti, &the_rti, sizeof(GF_SystemRTInfo));
	  return 0;
  }

  u_k_time = idle_time = 0;
  f = gf_f64_open("/proc/stat", "r");
  if (f) {
	  u32 k_time, nice_time, u_time;
	  if (fgets(line, 128, f) != NULL) {
		  if (sscanf(line, "cpu  %u %u %u %u\n", &u_time, &k_time, &nice_time, &idle_time) == 4) {
			  u_k_time = u_time + k_time + nice_time;
		  }
	  }
	  fclose(f);
  }

  process_u_k_time = 0;
  the_rti.process_memory = 0;

  /*FIXME? under LinuxThreads this will only fetch stats for the calling thread, we would have to enumerate /proc to get
  the complete CPU usage of all therads of the process...*/
#if 0
  sprintf(szProc, "/proc/%d/stat", the_rti.pid);
  f = gf_f64_open(szProc, "r");
  if (f) {
	  fflush(f);
	  if (fgets(line, 2048, f) != NULL) {
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
	  fclose(f);
  } else {
	  GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] cannot open %s\n", szProc));
  }
  sprintf(szProc, "/proc/%d/status", the_rti.pid);
  f = gf_f64_open(szProc, "r");
  if (f) {
	  while (fgets(line, 1024, f) != NULL) {
		  if (!strnicmp(line, "VmSize:", 7)) {
			  sscanf(line, "VmSize: %"LLD" kB",  &the_rti.process_memory);
			  the_rti.process_memory *= 1024;
		  }
	  }
	  fclose(f);
  } else {
	  GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] cannot open %s\n", szProc));
  }
#endif


  the_rti.physical_memory = the_rti.physical_memory_avail = 0;
  f = gf_f64_open("/proc/meminfo", "r");
  if (f) {
	  while (fgets(line, 1024, f) != NULL) {
		  if (!strnicmp(line, "MemTotal:", 9)) {
			  sscanf(line, "MemTotal: "LLU" kB",  &the_rti.physical_memory);
			  the_rti.physical_memory *= 1024;
		  }else if (!strnicmp(line, "MemFree:", 8)) {
			  sscanf(line, "MemFree: "LLU" kB",  &the_rti.physical_memory_avail);
			  the_rti.physical_memory_avail *= 1024;
			  break;
		  }
	  }
	  fclose(f);
  } else {
	  GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[RTI] cannot open /proc/meminfo\n"));
  }


  the_rti.sampling_instant = last_update_time;

  if (last_update_time) {
	  the_rti.sampling_period_duration = (entry_time - last_update_time);
	  the_rti.process_cpu_time_diff = (process_u_k_time - last_process_k_u_time) * 10;

	  /*oops, we have no choice but to assume 100% cpu usage during this period*/
	  if (!u_k_time) {
		  the_rti.total_cpu_time_diff = the_rti.sampling_period_duration;
		  u_k_time = last_cpu_u_k_time + the_rti.sampling_period_duration;
		  the_rti.cpu_idle_time = 0;
		  the_rti.total_cpu_usage = 100;
		  if (!the_rti.process_cpu_time_diff) the_rti.process_cpu_time_diff = the_rti.total_cpu_time_diff;
		  the_rti.process_cpu_usage = (u32) ( 100 *  the_rti.process_cpu_time_diff / the_rti.sampling_period_duration);
	  } else {
		  u64 samp_sys_time;
		  /*move to ms (/proc/stat gives times in 100 ms unit*/
		  the_rti.total_cpu_time_diff = (u_k_time - last_cpu_u_k_time)*10;

		  /*we're not that accurate....*/
		  if (the_rti.total_cpu_time_diff > the_rti.sampling_period_duration)
			  the_rti.sampling_period_duration = the_rti.total_cpu_time_diff;


		  if (!idle_time) idle_time = (the_rti.sampling_period_duration - the_rti.total_cpu_time_diff)/10;
		  samp_sys_time = u_k_time - last_cpu_u_k_time;
		  the_rti.cpu_idle_time = idle_time - last_cpu_idle_time;
		  the_rti.total_cpu_usage = (u32) ( 100 * samp_sys_time / (the_rti.cpu_idle_time + samp_sys_time ) );
		  /*move to ms (/proc/stat gives times in 100 ms unit*/
		  the_rti.cpu_idle_time *= 10;
		  if (!the_rti.process_cpu_time_diff) the_rti.process_cpu_time_diff = the_rti.total_cpu_time_diff;
		  the_rti.process_cpu_usage = (u32) ( 100 *  the_rti.process_cpu_time_diff / (the_rti.cpu_idle_time + 10*samp_sys_time ) );
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
  return 1;
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


char * gf_get_default_cache_directory(){  
#ifdef _WIN32_WCE
	return gf_strdup( "\\windows\\temp" );
#elif defined(WIN32)
	char szPath[MAX_PATH];
	/*ivica patch*/
	GetTempPath(MAX_PATH, szPath);
	return gf_strdup( szPath );
#else
	return gf_strdup("/tmp");
#endif
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
	if (onBattery) *onBattery = sps.ACLineStatus ? 0 : 1;
	if (onCharge) *onCharge = (sps.BatteryFlag & BATTERY_FLAG_CHARGING) ? 1 : 0;
	if (level) *level = sps.BatteryLifePercent;
	if (batteryLifeTime) *batteryLifeTime = sps.BatteryLifeTime;
	if (batteryFullLifeTime) *batteryFullLifeTime = sps.BatteryFullLifeTime;
#endif
	return 1;
}


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
		for (res = resourceName; *res ; res++){
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
	if (flags == -1){
		goto exit;
	}
	/* Set FD_CLOEXEC, so exclusive lock will be removed on exit, so even if GPAC crashes,
	* lock will be allowed for next instance */
	flags |= FD_CLOEXEC;
	/* Now, update the flags */
	if (fcntl(fd, F_SETFD, flags) == -1){
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

	if (ftruncate(fd, 0) == -1){
		goto exit;
	}
	/* Write the PID */
	{
		int sz = 100;
		char * buf = gf_malloc( sz );
		sz = snprintf(buf, sz, "%ld\n", (long) getpid());
		if (write(fd, buf, sz) != sz){
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
GF_GlobalLock * gf_global_resource_lock(const char * resourceName){
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
 * \param lock The resource to unlock
 * \return GF_OK if evertything went fine
 */
GF_EXPORT
GF_Err gf_global_resource_unlock(GF_GlobalLock * lock){
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

#ifdef GPAC_ANDROID

fm_callback_func fm_cbk = NULL;
static void *fm_cbk_obj = NULL;

void gf_fm_request_set_callback(void *cbk_obj, fm_callback_func cbk_func) {
	fm_cbk = cbk_func;
	fm_cbk_obj = cbk_obj;
}

void gf_fm_request_call(u32 type, u32 param, int *value) {
	if (fm_cbk)
		fm_cbk(fm_cbk_obj, type, param, value);
}

#endif //GPAC_ANDROID

/*
		NTP tools
*/
GF_EXPORT
void gf_net_get_ntp(u32 *sec, u32 *frac)
{
	u64 frac_part;
	struct timeval now;
#ifdef WIN32
	s32 gettimeofday(struct timeval *tp, void *tz);
#endif
	gettimeofday(&now, NULL);
	*sec = (u32) (now.tv_sec) + GF_NTP_SEC_1900_TO_1970;
//	*frac = (u32) ( (now.tv_usec << 12) + (now.tv_usec << 8) - ((now.tv_usec * 3650) >> 6) );
	frac_part = now.tv_usec * 0xFFFFFFFFULL;
	frac_part /= 1000000;
	*frac = (u32) ( frac_part );
}


GF_EXPORT
s32 gf_net_get_timezone()
{
#ifdef GPAC_ANDROID
	{
		/*FIXME - finad a safe way to estimate timezone this does not work !!*/
		s32 t_timezone;
		struct tm t_gmt, t_local;
		time_t t_time;
		t_time = time(NULL);
		t_gmt = *gmtime(&t_time);
		t_local = *localtime(&t_time);
	
		t_timezone = (t_gmt.tm_hour - t_local.tm_hour) * 3600;
		return t_timezone;
	}
#elif defined(_WIN32_WCE)
	return 0;
#else
	{
		s32 val = timezone;
		return val;
	}
#endif

}

GF_EXPORT
u64 gf_net_parse_date(const char *val)
{
	u64 current_time;
	char szDay[50], szMonth[50];
	u32 year, month, day, h, m, s, ms;
	s32 oh, om;
	Float secs;
	Bool neg_time_zone = 0;

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

	if (sscanf(val, "%d-%d-%dT%d:%d:%gZ", &year, &month, &day, &h, &m, &secs) == 6) {
	}
	else if (sscanf(val, "%d-%d-%dT%d:%d:%g-%d:%d", &year, &month, &day, &h, &m, &secs, &oh, &om) == 8) {
		neg_time_zone = 1;
	}
	else if (sscanf(val, "%d-%d-%dT%d:%d:%g+%d:%d", &year, &month, &day, &h, &m, &secs, &oh, &om) == 8) {
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
	else {
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
	current_time = mktime(&t) - gf_net_get_timezone();

#endif

	if (om || oh) {
		s32 diff = (60*oh + om)*60;
		if (neg_time_zone) diff = -diff;
		current_time = current_time + diff;
	}
	current_time *= 1000;
	ms = (u32) ( (secs - (u32) secs) * 1000);
	return current_time + ms;
}



GF_EXPORT
u64 gf_net_get_utc()
{
	u64 current_time;
	Double msec;
	u32 sec, frac;

#ifdef _WIN32_WCE
	SYSTEMTIME syst;
	FILETIME filet;
#else
	time_t gtime;
	struct tm _t;
#endif

	gf_net_get_ntp(&sec, &frac);

#ifndef _WIN32_WCE
	gtime = sec - GF_NTP_SEC_1900_TO_1970;
	_t = * gmtime(&gtime);
	current_time = mktime(&_t) - gf_net_get_timezone();
#else
	GetSystemTime(&syst);
	SystemTimeToFileTime(&syst, &filet);
	current_time = (u64) ((*(LONGLONG *) &filet - TIMESPEC_TO_FILETIME_OFFSET) / 10000000);
#endif

	current_time *= 1000;
	msec = frac*1000.0;
	msec /= 0xFFFFFFFF;
	current_time += (u64) msec;
	return current_time;
}



GF_EXPORT
GF_Err gf_bin128_parse(char *string, bin128 value)
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
			value[i] = v;
			i++;
		}
	}
	if (i != 16) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[CORE] 128bit blob is not 16-bytes long: %s\n", string));
		return GF_BAD_PARAM;
	}
	return GF_OK;
}
