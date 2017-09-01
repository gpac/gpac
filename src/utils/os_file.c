/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre - Copyright (c) Telecom ParisTech 2000-2012
 *			         Romain Bouqueau - Copyright (c) Romain Bouqueau 2015
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
#include <gpac/utf.h>

#if defined(_WIN32_WCE)

#include <winbase.h>
#include <tlhelp32.h>

#elif defined(WIN32)

#include <windows.h>
#include <direct.h>
#include <sys/stat.h>
#include <share.h>

#else

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <time.h>
#include <sys/time.h>

#ifndef __BEOS__
#include <errno.h>
#endif

#endif


GF_Err gf_rmdir(char *DirPathName)
{
#if defined (_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	BOOL res;
	CE_CharToWide(DirPathName, swzName);
	res = RemoveDirectory(swzName);
	if (! res) {
		int err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot delete directory %s: last error %d\n", DirPathName, err ));
	}
#elif defined (WIN32)
	int res = rmdir(DirPathName);
	if (res==-1) {
		int err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot delete directory %s: last error %d\n", DirPathName, err ));
		return GF_IO_ERR;
	}
#else
	int res = rmdir(DirPathName);
	if (res==-1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot delete directory %s: last error %d\n", DirPathName, errno  ));
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
		if(errno == 17) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create directory %s, it already exists: last error %d \n", DirPathName, errno ));
			return GF_BAD_PARAM;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create directory %s: last error %d\n", DirPathName, errno ));
			return GF_IO_ERR;
		}
	}
#endif
	return GF_OK;
}


GF_EXPORT
Bool gf_dir_exists(char* DirPathName)
{
#if defined (_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	BOOL res;
	DWORD att;
	CE_CharToWide(DirPathName, swzName);
	att = GetFileAttributes(swzName);
	return (att != INVALID_FILE_ATTRIBUTES && (att & FILE_ATTRIBUTE_DIRECTORY)) ? GF_TRUE : GF_FALSE;
#elif defined (WIN32)
	DWORD att = GetFileAttributes(DirPathName);
	return (att != INVALID_FILE_ATTRIBUTES && (att & FILE_ATTRIBUTE_DIRECTORY)) ? GF_TRUE : GF_FALSE;
#else
	DIR* dir = opendir(DirPathName);
	if (!dir) return GF_FALSE;
	closedir(dir);
	return GF_TRUE;
#endif
	return GF_FALSE;
}
static Bool delete_dir(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	Bool directory_clean_mode = *(Bool*)cbck;

	if(directory_clean_mode) {
		gf_cleanup_dir(item_path);
		gf_rmdir(item_path);
	} else {
		gf_delete_file(item_path);
	}
	return GF_FALSE;
}

GF_Err gf_cleanup_dir(char* DirPathName)
{
	Bool directory_clean_mode;

	directory_clean_mode = GF_TRUE;
	gf_enum_directory(DirPathName, GF_TRUE, delete_dir, &directory_clean_mode, NULL);
	directory_clean_mode = GF_FALSE;
	gf_enum_directory(DirPathName, GF_FALSE, delete_dir, &directory_clean_mode, NULL);

	return GF_OK;
}

GF_EXPORT
GF_Err gf_delete_file(const char *fileName)
{
	if (!fileName) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("gf_delete_file deletes nothing - ignoring\n"));
		return GF_OK;
	}
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

#ifndef WIN32
/**
 * Remove existing single-quote from a single-quoted string.
 * The caller is responsible for deallocating the returns string with gf_free()
 */
static char* gf_sanetize_single_quoted_string(const char *src) {
	int i, j;
	char *out = (char*)gf_malloc(4*strlen(src)+3);
	out[0] = '\'';
	for (i=0, j=1; (out[j]=src[i]); ++i, ++j) {
		if (src[i]=='\'') {
			out[++j]='\\';
			out[++j]='\'';
			out[++j]='\'';
		}
	}
	out[j++] = '\'';
	out[j++] = 0;
	return out;
}
#endif

#if defined(GPAC_IPHONE) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= 80000)
#include <spawn.h>
extern char **environ;
#endif

GF_EXPORT
Bool gf_file_exists(const char *fileName)
{
	FILE *f = gf_fopen(fileName, "r");
	if (f) {
		gf_fclose(f);
		return GF_TRUE;
	}
	return GF_FALSE;
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
	GF_Err e = GF_IO_ERR;
	char cmd[1024], *arg1, *arg2;
	if (!fileName || !newFileName)
		return GF_IO_ERR;
	arg1 = gf_sanetize_single_quoted_string(fileName);
	arg2 = gf_sanetize_single_quoted_string(newFileName);
	if (snprintf(cmd, sizeof cmd, "mv %s %s", arg1, arg2) >= sizeof cmd) goto error;

#if defined(GPAC_IPHONE) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= 80000)
	{
		pid_t pid;
		char *argv[3];
		argv[0] = "mv";
		argv[1] = cmd;
		argv[2] = NULL;
		posix_spawn(&pid, argv[0], NULL, NULL, argv, environ);
		waitpid(pid, NULL, 0);
	}
#else
	e = (system(cmd) == 0) ? GF_OK : GF_IO_ERR;
#endif

error:
	gf_free(arg1);
	gf_free(arg2);
	return e;
#endif
}

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

static u32 gpac_file_handles = 0;
GF_EXPORT
u32 gf_file_handles_count()
{
	return gpac_file_handles;
}

GF_EXPORT
FILE *gf_temp_file_new(char ** const fileName)
{
	FILE *res = NULL;
#if defined(_WIN32_WCE)
	TCHAR pPath[MAX_PATH+1];
	TCHAR pTemp[MAX_PATH+1];
	if (!GetTempPath(MAX_PATH, pPath)) {
		pPath[0] = '.';
		pPath[1] = '.';
	}
	if (GetTempFileName(pPath, TEXT("git"), 0, pTemp))
		res = _wfopen(pTemp, TEXT("w+b"));
#elif defined(WIN32)
	char tmp[MAX_PATH];
	res = tmpfile();
	if (!res) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Win32] system failure for tmpfile(): 0x%08x\n", GetLastError()));

		/*tmpfile() may fail under vista ...*/
		if (GetEnvironmentVariable("TEMP", tmp, MAX_PATH)) {
			char tmp2[MAX_PATH], *t_file;
			gf_rand_init(GF_FALSE);
			sprintf(tmp2, "gpac_%08x_", gf_rand());
			t_file = tempnam(tmp, tmp2);
			res = gf_fopen(t_file, "w+b");
			if (res) {
				gpac_file_handles--;
				if (fileName) {
					*fileName = gf_strdup(t_file);
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Win32] temporary file %s won't be deleted - contact the GPAC team\n", t_file));
				}
			}
			free(t_file);
		}
	}
#else
	res = tmpfile();
#endif

	if (res) {
		gpac_file_handles++;
	}
	return res;
}

/*enumerate directories*/
GF_EXPORT
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, gf_enum_dir_item enum_dir_fct, void *cbck, const char *filter)
{
	char item_path[GF_MAX_PATH];
	GF_FileEnumInfo file_info;

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

	memset(&file_info, 0, sizeof(GF_FileEnumInfo) );

	if (!strcmp(dir, "/")) {
#if defined(WIN32) && !defined(_WIN32_WCE)
		u32 len;
		char *drives, *volume;
		len = GetLogicalDriveStrings(0, NULL);
		drives = (char*)gf_malloc(sizeof(char)*(len+1));
		drives[0]=0;
		GetLogicalDriveStrings(len, drives);
		len = (u32) strlen(drives);
		volume = drives;
		file_info.directory = GF_TRUE;
		file_info.drive = GF_TRUE;
		while (len) {
			enum_dir_fct(cbck, volume, "", &file_info);
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
		for (TInt i=0; i<KMaxDrives; i++) {
			if (aList[i]) {
				char szDrive[10];
				TChar aDrive;
				iFs.DriveToChar(i, aDrive);
				sprintf(szDrive, "%c:", (TUint)aDrive);
				enum_dir_fct(cbck, szDrive, "", &file_info);
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
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot open directory %s for enumeration: %d\n", path, errno));
		return GF_IO_ERR;
	}
	the_file = readdir(the_dir);
	while (the_file) {

#endif

		memset(&file_info, 0, sizeof(GF_FileEnumInfo) );


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
		file_info.directory = (FindData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) ? GF_TRUE : GF_FALSE;
		if (!enum_directory && file_info.directory) goto next;
		if (enum_directory && !file_info.directory) goto next;
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

#if defined(WIN32)
		file_info.hidden = (FindData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? GF_TRUE : GF_FALSE;
		file_info.system = (FindData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ? GF_TRUE : GF_FALSE;
		file_info.size = MAXDWORD;
		file_info.size += 1;
		file_info.size *= FindData.nFileSizeHigh;
		file_info.size += FindData.nFileSizeLow;
		file_info.last_modified = (u64) ((*(LONGLONG *) &FindData.ftLastWriteTime - TIMESPEC_TO_FILETIME_OFFSET) / 10000000);
#endif

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

		file_info.directory = ((st.st_mode & S_IFMT) == S_IFDIR) ? GF_TRUE : GF_FALSE;
		if (enum_directory && !file_info.directory) goto next;
		if (!enum_directory && file_info.directory) goto next;

		file_info.size = st.st_size;

		{
			struct tm _t = * gmtime(& st.st_mtime);
			file_info.last_modified = mktime(&_t);
		}
		file = the_file->d_name;
		if (file && file[0]=='.') file_info.hidden = 1;

		if (file_info.directory) {
			char * parent_name = strrchr(item_path, '/');
			if (!parent_name) {
				file_info.drive = GF_TRUE;
			} else {
				struct stat st_parent;
				parent_name[0] = 0;
				if (stat(item_path, &st_parent) == 0)  {
					if ((st.st_dev != st_parent.st_dev) || ((st.st_dev == st_parent.st_dev) && (st.st_ino == st_parent.st_ino))) {
						file_info.drive = GF_TRUE;
					}
				}
				parent_name[0] = '/';
			}
		}
#endif
		if (enum_dir_fct(cbck, file, item_path, &file_info)) {
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

GF_EXPORT
u64 gf_ftell(FILE *fp)
{
#if defined(_WIN32_WCE)
	return (u64) ftell(fp);
#elif defined(GPAC_CONFIG_WIN32) && !defined(__CYGWIN__)	/* mingw or cygwin */
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
u64 gf_fseek(FILE *fp, s64 offset, s32 whence)
{
#if defined(_WIN32_WCE)
	return (u64) fseek(fp, (s32) offset, whence);
#elif defined(GPAC_CONFIG_WIN32) && !defined(__CYGWIN__)	/* mingw or cygwin */
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
FILE *gf_fopen(const char *file_name, const char *mode)
{
	FILE *res = NULL;

#if defined(WIN32)
	Bool is_create;
	is_create = (strchr(mode, 'w') == NULL) ? GF_FALSE : GF_TRUE;
	if (!is_create) {
		if (strchr(mode, 'a')) {
			res = fopen(file_name, "rb");
			if (res) {
				fclose(res);
				res = fopen(file_name, mode);
			}
		} else {
			res = fopen(file_name, mode);
		}
	}
	if (!res) {
		const char *str_src;
		wchar_t *wname;
		wchar_t *wmode;
		size_t len;
		size_t len_res;
		if (!is_create) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Core] Could not open file %s mode %s in UTF-8 mode, trying UTF-16\n", file_name, mode));
		}
		len = (strlen(file_name) + 1)*sizeof(wchar_t);
		wname = (wchar_t *)gf_malloc(len);
		str_src = file_name;
		len_res = gf_utf8_mbstowcs(wname, len, &str_src);
		if (len_res == -1) {
			return NULL;
		}
		len = (strlen(mode) + 1)*sizeof(wchar_t);
		wmode = (wchar_t *)gf_malloc(len);
		str_src = mode;
		len_res = gf_utf8_mbstowcs(wmode, len, &str_src);
		if (len_res == -1) {
			return NULL;
		}

		res = _wfsopen(wname, wmode, _SH_DENYNO);
		gf_free(wname);
		gf_free(wmode);
	}
#elif defined(GPAC_CONFIG_LINUX) && !defined(GPAC_ANDROID)
	res = fopen64(file_name, mode);
#elif (defined(GPAC_CONFIG_FREEBSD) || defined(GPAC_CONFIG_DARWIN))
	res = fopen(file_name, mode);
#else
	res = fopen(file_name, mode);
#endif

	if (res) {
		gpac_file_handles++;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] file %s opened in mode %s - %d file handles\n", file_name, mode, gpac_file_handles));
	} else {
		if (strchr(mode, 'w') || strchr(mode, 'a')) {
#if defined(WIN32)
			u32 err = GetLastError();
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] system failure for file opening of %s in mode %s: 0x%08x\n", file_name, mode, err));
#else
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] system failure for file opening of %s in mode %s: %d\n", file_name, mode, errno));
#endif
		}
	}
	return res;
}

GF_EXPORT
s32 gf_fclose(FILE *file)
{
	if (file) {
		assert(gpac_file_handles);
		gpac_file_handles--;
	}
	return fclose(file);
}

#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! defined(_GNU_SOURCE) && !defined(WIN32)
#define HAVE_STRERROR_R 1
#endif

GF_EXPORT
size_t gf_fread(void *ptr, size_t size, size_t nmemb, FILE *stream) {
	return fread(ptr, size, nmemb, stream);
}

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


/**
  * Returns a pointer to the start of a filepath basename
 **/
GF_EXPORT
char* gf_file_basename(const char* filename)
{
	char* lastPathPart = NULL;
	if (filename) {

		lastPathPart = strrchr(filename , GF_PATH_SEPARATOR);
		if (GF_PATH_SEPARATOR != '/')
		{
			// windows paths can mix slashes and backslashes
			// so we search for the last slash that occurs after the last backslash
			// if it occurs before it's not relevant
			// if there's no backslashes we search in the whole file path

			char* trailingSlash = strrchr(lastPathPart?lastPathPart:filename, '/');
			if (trailingSlash)
				lastPathPart = trailingSlash;
		}
		if (!lastPathPart)
			lastPathPart = (char *)filename;
		else
			lastPathPart++;



	}
	return lastPathPart;
}

/**
  * Returns a pointer to the start of a filepath extension or null
 **/
GF_EXPORT
char* gf_file_ext_start(const char* filename)
{
	char* res = NULL;
	char* basename = gf_file_basename(filename);
	if (basename) {

		res = strrchr(basename, '.');

	}
	return res;
}
