/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre - Copyright (c) Telecom ParisTech 2000-2023
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
#include <process.h>
#include <direct.h>
#include <sys/stat.h>
#include <share.h>

#else

#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/time.h>

#ifndef __BEOS__
#include <errno.h>
#endif

#endif


GF_EXPORT
GF_Err gf_rmdir(const char *DirPathName)
{
#if defined (_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	BOOL res;
	CE_CharToWide(DirPathName, swzName);
	res = RemoveDirectory(swzName);
	if (! res) {
		int err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot delete directory \"%s\": last error %d\n", DirPathName, err ));
	}
#elif defined (WIN32)
	int res;
	wchar_t* wcsDirPathName = gf_utf8_to_wcs(DirPathName);
	if (!wcsDirPathName)
		return GF_IO_ERR;
	res = _wrmdir(wcsDirPathName);
	gf_free(wcsDirPathName);
	if (res == -1) {
		int err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot delete directory \"%s\": last error %d\n", DirPathName, err ));
		return GF_IO_ERR;
	}
#else
	int res = rmdir(DirPathName);
	if (res==-1) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot delete directory \"%s\": last error %d\n", DirPathName, errno  ));
		return GF_IO_ERR;
	}
#endif
	return GF_OK;
}

GF_EXPORT
GF_Err gf_mkdir(const char* DirPathName)
{
#if defined (_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	BOOL res;
	CE_CharToWide(DirPathName, swzName);
	res = CreateDirectory(swzName, NULL);
	if (! res) {
		int err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create directory \"%s\": last error %d\n", DirPathName, err ));
	}
#elif defined (WIN32)
	int res;

	//don't try creation for UNC root:  \\foo\bar\name will trigger creation for "", "\\foo" and "\\foo\bar", ignore these
	if (!strcmp(DirPathName, "")) return GF_OK;
	if (!strncmp(DirPathName, "\\\\", 2)) {
		char *sep = strchr(DirPathName + 2, '\\');
		if (!sep) return GF_OK;
		sep = strchr(sep + 1, '\\');
		if (!sep) return GF_OK;
	}

	wchar_t* wcsDirPathName = gf_utf8_to_wcs(DirPathName);
	if (!wcsDirPathName)
		return GF_IO_ERR;
	res = _wmkdir(wcsDirPathName);
	gf_free(wcsDirPathName);
	if (res==-1) {
		int err = GetLastError();
		if (err != 183) {
			// don't throw error if dir already exists
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create directory \"%s\": last error %d\n", DirPathName, err));
		}
	}
#else
	int res = mkdir(DirPathName, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
	if (res==-1) {
		if(errno == 17) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Cannot create directory \"%s\", it already exists: last error %d \n", DirPathName, errno ));
			return GF_OK;
		} else {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create directory \"%s\": last error %d\n", DirPathName, errno ));
			return GF_IO_ERR;
		}
	}
#endif
	return GF_OK;
}


GF_EXPORT
Bool gf_dir_exists(const char* DirPathName)
{
#if defined (_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	DWORD att;
	CE_CharToWide(DirPathName, swzName);
	att = GetFileAttributes(swzName);
	return (att != INVALID_FILE_ATTRIBUTES && (att & FILE_ATTRIBUTE_DIRECTORY)) ? GF_TRUE : GF_FALSE;
#elif defined (WIN32)
	DWORD att;
	wchar_t* wcsDirPathName = gf_utf8_to_wcs(DirPathName);
	if (!wcsDirPathName)
		return GF_FALSE;
	att = GetFileAttributesW(wcsDirPathName);
	gf_free(wcsDirPathName);
	return (att != INVALID_FILE_ATTRIBUTES && (att & FILE_ATTRIBUTE_DIRECTORY)) ? GF_TRUE : GF_FALSE;
#else
	struct stat sb;
	if (stat(DirPathName, &sb) == 0 && S_ISDIR(sb.st_mode)) {
		return GF_TRUE;
	}
	return GF_FALSE;
//	DIR* dir = opendir(DirPathName);
//	if (!dir) return GF_FALSE;
//	closedir(dir);
//	return GF_TRUE;
#endif
	return GF_FALSE;
}
static Bool delete_dir(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	Bool directory_clean_mode = *(Bool*)cbck;

	if(directory_clean_mode) {
		gf_dir_cleanup(item_path);
		gf_rmdir(item_path);
	} else {
		gf_file_delete(item_path);
	}
	return GF_FALSE;
}

GF_EXPORT
GF_Err gf_dir_cleanup(const char* DirPathName)
{
	Bool directory_clean_mode;

	directory_clean_mode = GF_TRUE;
	gf_enum_directory(DirPathName, GF_TRUE, delete_dir, &directory_clean_mode, NULL);
	directory_clean_mode = GF_FALSE;
	gf_enum_directory(DirPathName, GF_FALSE, delete_dir, &directory_clean_mode, NULL);

	return GF_OK;
}

GF_EXPORT
GF_Err gf_file_delete(const char *fileName)
{
	if (!fileName || !fileName[0]) {
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("gf_file_delete with no param - ignoring\n"));
		return GF_OK;
	}
#if defined(_WIN32_WCE)
	TCHAR swzName[MAX_PATH];
	CE_CharToWide((char*)fileName, swzName);
	return (DeleteFile(swzName)==0) ? GF_IO_ERR : GF_OK;
#elif defined(WIN32)
	/* success if != 0 */
	{
		BOOL op_result;
		wchar_t* wcsFileName = gf_utf8_to_wcs(fileName);
		if (!wcsFileName)
			return GF_IO_ERR;
		op_result = DeleteFileW(wcsFileName);
		gf_free(wcsFileName);
		return (op_result==0) ? GF_IO_ERR : GF_OK;
	}
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

#if defined(GPAC_CONFIG_IOS) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= 80000)
#include <spawn.h>
extern char **environ;
#endif

#if defined(WIN32)
#include <io.h>
#endif

GF_EXPORT
Bool gf_file_exists_ex(const char *fileName, const char *par_name)
{
	u32 gfio_type = 0;
	if (!fileName) return GF_FALSE;

	if (!strncmp(fileName, "gfio://", 7))
		gfio_type = 1;
	else if (par_name && !strncmp(par_name, "gfio://", 7))
		gfio_type = 2;

	if (gfio_type) {
		GF_FileIO *gfio_ref;
		GF_Err e;
		Bool res = GF_TRUE;
		if (gfio_type==1)
			gfio_ref = gf_fileio_from_url(fileName);
		else
			gfio_ref = gf_fileio_from_url(par_name);

		if (!gfio_ref) return GF_FALSE;
		gf_fileio_open_url(gfio_ref, (gfio_type==1) ? gf_fileio_resource_url(gfio_ref) : fileName, "probe", &e);
		if (e==GF_URL_ERROR)
			res = GF_FALSE;
		return res;
	}

#if defined(WIN32)
	Bool res;
	wchar_t* wname = gf_utf8_to_wcs(fileName);
	if (!wname) return GF_FALSE;
 	res = (_waccess(wname, 4) == -1) ? GF_FALSE : GF_TRUE;
	if (res == GF_TRUE) {
		DWORD att;
		att = GetFileAttributesW(wname);
		if (att != INVALID_FILE_ATTRIBUTES && (att & FILE_ATTRIBUTE_DIRECTORY))
			res = GF_FALSE;
	}
	gf_free(wname);
	return res;
#elif defined(GPAC_CONFIG_LINUX) || defined(GPAC_CONFIG_EMSCRIPTEN)
	Bool res = (access(fileName, 4) == -1) ? GF_FALSE : GF_TRUE;
	if (res && gf_dir_exists(fileName))
		res = GF_FALSE;
	return res;
#elif defined(__DARWIN__) || defined(__APPLE__)
 	Bool res = (access(fileName, 4) == -1) ? GF_FALSE : GF_TRUE;
	if (res && gf_dir_exists(fileName))
		res = GF_FALSE;
	return res;
#elif defined(GPAC_CONFIG_IOS) || defined(GPAC_CONFIG_ANDROID)
 	Bool res = (access(fileName, 4) == -1) ? GF_FALSE : GF_TRUE;
	if (res && gf_dir_exists(fileName))
		res = GF_FALSE;
	return res;
#else
	FILE *f = gf_fopen(fileName, "r");
	if (f) {
		gf_fclose(f);
		return GF_TRUE;
	}
	return GF_FALSE;
#endif
}

GF_EXPORT
Bool gf_file_exists(const char *fileName)
{
	return gf_file_exists_ex(fileName, NULL);
}

GF_EXPORT
GF_Err gf_file_move(const char *fileName, const char *newFileName)
{
#if defined(_WIN32_WCE)
	GF_Err e = GF_OK;
	TCHAR swzName[MAX_PATH];
	TCHAR swzNewName[MAX_PATH];
	CE_CharToWide((char*)fileName, swzName);
	CE_CharToWide((char*)newFileName, swzNewName);
	if (MoveFile(swzName, swzNewName) == 0 )
		e = GF_IO_ERR;
#elif defined(WIN32)
	GF_Err e = GF_OK;
	/* success if != 0 */
	BOOL op_result;
	wchar_t* wcsFileName = gf_utf8_to_wcs(fileName);
	wchar_t* wcsNewFileName = gf_utf8_to_wcs(newFileName);
	if (!wcsFileName || !wcsNewFileName) {
		if (wcsFileName) gf_free(wcsFileName);
		if (wcsNewFileName) gf_free(wcsNewFileName);
		e = GF_IO_ERR;
	} else {
		op_result = MoveFileW(wcsFileName, wcsNewFileName);
		gf_free(wcsFileName);
		gf_free(wcsNewFileName);
		if ( op_result == 0 )
			e = GF_IO_ERR;
	}
#else
	GF_Err e = GF_IO_ERR;
	char cmd[1024], *arg1, *arg2;
	if (!fileName || !newFileName) {
		e = GF_IO_ERR;
	} else {
		arg1 = gf_sanetize_single_quoted_string(fileName);
		arg2 = gf_sanetize_single_quoted_string(newFileName);
		if (snprintf(cmd, sizeof cmd, "mv %s %s", arg1, arg2) >= sizeof cmd) goto error;

#if defined(GPAC_CONFIG_IOS) && (__IPHONE_OS_VERSION_MAX_ALLOWED >= 80000)
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
	}
#endif

	if (e) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] Failed to move file %s to %s: %s\n", fileName, newFileName, gf_error_to_string(e) ));
	}
	return e;
}

GF_EXPORT
u64 gf_file_modification_time(const char *filename)
{
#if defined(_WIN32_WCE)
	WCHAR _file[GF_MAX_PATH];
	WIN32_FIND_DATA FindData;
	HANDLE fh;
//	ULARGE_INTEGER uli;
	ULONGLONG time_us;
	BOOL ret;
	CE_CharToWide((char *) filename, _file);
	fh = FindFirstFile(_file, &FindData);
	if (fh == INVALID_HANDLE_VALUE) return 0;
//	uli.LowPart = FindData.ftLastWriteTime.dwLowDateTime;
//	uli.HighPart = FindData.ftLastWriteTime.dwHighDateTime;
	time_us = (u64) ((*(LONGLONG *) &FindData.ftLastWriteTime - TIMESPEC_TO_FILETIME_OFFSET) / 10);
	ret = FindClose(fh);
	if (!ret) {
		DWORD err = GetLastError();
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] FindClose() in gf_file_modification_time() error: %d\n", err));
	}
//	time_ms = uli.QuadPart/10000;
	return time_us;
#elif defined(WIN32) && !defined(__GNUC__)
	struct _stat64 st;
	int op_result;
	wchar_t* wcsFilename = gf_utf8_to_wcs(filename);
	if (!wcsFilename)
		return 0;
	op_result = _wstat64(wcsFilename, &st);
	gf_free(wcsFilename);
	if (op_result != 0) return 0;
	u64 time_us = st.st_mtime * 1000000;
#if defined(GPAC_HAS_MTIM_NSEC)
	time_us += st.st_mtim.tv_nsec / 1000;
#endif
	return time_us;
#else
	struct stat st;
	if (stat(filename, &st) != 0) return 0;
	u64 time_us = st.st_mtime * 1000000;

#if defined(__DARWIN__) || defined(__APPLE__) || defined(GPAC_CONFIG_IOS)
	time_us += st.st_mtimespec.tv_nsec / 1000;
#elif defined(GPAC_HAS_MTIM_NSEC)
	time_us += st.st_mtim.tv_nsec / 1000;
#elif defined(GPAC_CONFIG_ANDROID)
	time_us += st.st_mtime_nsec / 1000;
#endif

	return time_us;
#endif
	return 0;
}

#include <gpac/list.h>
extern int gf_mem_track_enabled;
static  GF_List * gpac_open_files = NULL;

typedef struct
{
	FILE *ptr;
	char *url;
	Bool is_temp;
} GF_FileHandle;

static u32 gpac_file_handles = 0;



GF_EXPORT
u32 gf_file_handles_count()
{
#ifdef GPAC_MEMORY_TRACKING
	if (gpac_open_files) {
		return gf_list_count(gpac_open_files);
	}
#endif
	return gpac_file_handles;
}

#include <gpac/thread.h>
extern GF_Mutex *logs_mx;

#ifdef GPAC_MEMORY_TRACKING
const char *enum_open_handles(u32 *idx)
{
	GF_FileHandle *h;
	gf_mx_p(logs_mx);
	u32 count = gf_list_count(gpac_open_files);
	if (*idx >= count) {
		gf_mx_v(logs_mx);
		return NULL;
	}
	h = gf_list_get(gpac_open_files, *idx);
	(*idx)++;
	gf_mx_v(logs_mx);
	return h->url;
}
#endif

static void gf_register_file_handle(char *filename, FILE *ptr, Bool is_temp_file)
{
	if (is_temp_file
#ifdef GPAC_MEMORY_TRACKING
		|| gf_mem_track_enabled
#endif
	) {
		GF_FileHandle *h;
		gf_mx_p(logs_mx);
		if (!gpac_open_files) gpac_open_files = gf_list_new();
		GF_SAFEALLOC(h, GF_FileHandle);
		if (h) {
			h->ptr = ptr;
			if (is_temp_file) {
				h->is_temp = GF_TRUE;
				h->url = filename;
			} else {
				h->url = gf_strdup(filename);
			}
			gf_list_add(gpac_open_files, h);
		}
		gf_mx_v(logs_mx);
	}
	gpac_file_handles++;
}

static Bool gf_unregister_file_handle(FILE *ptr)
{
	u32 i, count;
	Bool res = GF_FALSE;
	if (gpac_file_handles)
		gpac_file_handles--;

	if (!gpac_open_files)
		return GF_FALSE;

	gf_mx_p(logs_mx);
	count = gf_list_count(gpac_open_files);
	for (i=0; i<count; i++) {
		GF_FileHandle *h = gf_list_get(gpac_open_files, i);
		if (h->ptr != ptr) continue;

		if (h->is_temp) {
			GF_Err e;
			fclose(h->ptr);
			e = gf_file_delete(h->url);
			if (e) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] Failed to delete temp file %s: %s\n", h->url, gf_error_to_string(e)));
			}
			res = GF_TRUE;
		}
		gf_free(h->url);
		gf_free(h);
		gf_list_rem(gpac_open_files, i);
		if (!gf_list_count(gpac_open_files)) {
			gf_list_del(gpac_open_files);
			gpac_open_files = NULL;
		}
		break;
	}
	gf_mx_v(logs_mx);
	return res;
}

static FILE *gf_file_temp_os(char ** const fileName)
{
	FILE *res;
#if defined(_WIN32_WCE)
	TCHAR pPath[MAX_PATH+1];
	TCHAR pTemp[MAX_PATH+1];
	res = NULL;
	if (!GetTempPath(MAX_PATH, pPath)) {
		pPath[0] = '.';
		pPath[1] = '.';
	}
	if (GetTempFileName(pPath, TEXT("git"), 0, pTemp))
		res = _wfopen(pTemp, TEXT("w+b"));
#elif defined(WIN32)
	res = tmpfile();

	if (!res) {
		wchar_t tmp[MAX_PATH];

		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[Win32] system failure for tmpfile(): 0x%08x\n", GetLastError()));

		/*tmpfile() may fail under vista ...*/
		if (GetEnvironmentVariableW(L"TEMP", tmp, MAX_PATH)) {
			wchar_t tmp2[MAX_PATH], *t_file;
			char* mbs_t_file;
			gf_rand_init(GF_FALSE);
			swprintf(tmp2, MAX_PATH, L"gpac_%d_%08x_", _getpid(), gf_rand());
			t_file = _wtempnam(tmp, tmp2);
			mbs_t_file = gf_wcs_to_utf8(t_file);
			if (!mbs_t_file)
				return 0;
			res = gf_fopen(mbs_t_file, "w+b");
			if (res) {
				gf_unregister_file_handle(res);
				if (fileName) {
					*fileName = gf_strdup(mbs_t_file);
				} else {
					GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Win32] temporary file \"%s\" won't be deleted - contact the GPAC team\n", mbs_t_file));
				}
			}
			gf_free(mbs_t_file);
			free(t_file);
		}
	}
#else
	res = tmpfile();
#endif

	if (res) {
		gf_register_file_handle("temp file", res, GF_FALSE);
	}
	return res;
}

GF_EXPORT
FILE *gf_file_temp(char ** const fileName)
{
	const char *tmp_dir = gf_opts_get_key("core", "tmp");

	if (tmp_dir && tmp_dir[0]) {
		FILE *res;
		char *opath=NULL, szFName[100];
		u32 len = (u32) strlen(tmp_dir);

		gf_dynstrcat(&opath, tmp_dir, NULL);
		if (!strchr("/\\", tmp_dir[len-1]))
			gf_dynstrcat(&opath, "/", NULL);
		if (!opath) return NULL;

		sprintf(szFName, "_libgpac_%d_%p_" LLU "_%d", gf_sys_get_process_id(), opath, gf_sys_clock_high_res(), gf_rand() );
		gf_dynstrcat(&opath, szFName, NULL);
		if (fileName) {
			sprintf(szFName, "%p", fileName);
			gf_dynstrcat(&opath, szFName, "_");
		}

		if (gf_file_exists(opath)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Something went wrong creating temp file path %s, file already exists !\n", opath));
			gf_free(opath);
			return NULL;
		}
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Opening new temp file %s\n", opath));
		res = gf_fopen_ex(opath, "__temp_file", "w+b", GF_FALSE);
		if (!res) {
			gf_free(opath);
			return NULL;
		}
		gf_register_file_handle(opath, res, GF_TRUE);
		return res;
	}
	return gf_file_temp_os(fileName);
}


/*enumerate directories*/
GF_EXPORT
GF_Err gf_enum_directory(const char *dir, Bool enum_directory, gf_enum_dir_item enum_dir_fct, void *cbck, const char *filter)
{
#ifdef WIN32
	wchar_t item_path[GF_MAX_PATH];
#else
	char item_path[GF_MAX_PATH];
#endif
	GF_FileEnumInfo file_info;

#if defined(_WIN32_WCE)
	char _path[GF_MAX_PATH];
	unsigned short path[GF_MAX_PATH];
	unsigned short w_filter[GF_MAX_PATH];
	char file[GF_MAX_PATH];
#elif defined(WIN32)
	wchar_t path[GF_MAX_PATH], *file;
	wchar_t w_filter[GF_MAX_PATH];
	char *mbs_file, *mbs_item_path;
	char _path[GF_MAX_PATH];
	const char* tmpdir;
#else
	char path[GF_MAX_PATH], *file;
#endif

#ifdef WIN32
	WIN32_FIND_DATAW FindData;
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
#elif defined(GPAC_CONFIG_ANDROID) || defined(GPAC_CONFIG_IOS)
		dir = (char *) gf_opts_get_key("core", "docs-dir");
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

	strcpy(_path, dir);
	switch (dir[strlen(dir)] - 1) {
	case '/':
	case '\\':
		snprintf(_path, MAX_PATH, "%s*", dir);
		break;
	default:
		snprintf(_path, MAX_PATH, "%s%c*", dir, GF_PATH_SEPARATOR);
		break;
	}

	tmpdir = _path;
	if (gf_utf8_mbstowcs(path, GF_MAX_PATH, &tmpdir) == GF_UTF8_FAIL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot convert %s to UTF16: broken string\n", dir));
		return GF_BAD_PARAM;
	}
	tmpdir  = filter;
	if (gf_utf8_mbstowcs(w_filter, sizeof(w_filter), &tmpdir) == GF_UTF8_FAIL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot convert %s to UTF16: broken string\n", filter));
		return GF_BAD_PARAM;
	}

#else
	strcpy(path, dir);
	if (path[strlen(path)-1] != '/') strcat(path, "/");
#endif

#ifdef WIN32
	SearchH= FindFirstFileW(path, &FindData);
	if (SearchH == INVALID_HANDLE_VALUE) return GF_IO_ERR;

#if defined (_WIN32_WCE)
	_path[strlen(_path)-1] = 0;
#else
	path[wcslen(path)-1] = 0;
#endif

	while (SearchH != INVALID_HANDLE_VALUE) {

#else

	the_dir = opendir(path);
	if (the_dir == NULL) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Cannot open directory \"%s\" for enumeration: %d\n", path, errno));
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
		if (!wcscmp(FindData.cFileName, L".")) goto next;
		if (!wcscmp(FindData.cFileName, L"..")) goto next;
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
			short *found_ext;
			u32 ext_len;
			if (!sep) goto next;
			wcscpy(ext, sep+1);
			wcslwr(ext);
			ext_len = (u32) wcslen(ext);
			found_ext = wcsstr(w_filter, ext);
			if (!found_ext) goto next;
			if (found_ext[ext_len] && (found_ext[ext_len] != (wchar_t) ';')) goto next;
#elif defined(WIN32)
			wchar_t ext[30];
			wchar_t *sep = wcsrchr(FindData.cFileName, L'.');
			wchar_t *found_ext;
			u32 ext_len;
			if (!sep) goto next;
			wcscpy(ext, sep+1);
			wcslwr(ext);
			ext_len = (u32) wcslen(ext);
			found_ext = wcsstr(w_filter, ext);
			if (!found_ext) goto next;
			if (found_ext[ext_len] && (found_ext[ext_len] != L';')) goto next;
#else
			char ext[30];
			char *sep = strrchr(the_file->d_name, '.');
			char *found_ext;
			u32 ext_len;
			if (!sep) goto next;
			strcpy(ext, sep+1);
			strlwr(ext);
			ext_len = (u32) strlen(ext);
			found_ext = strstr(filter, ext);
			if (!found_ext) goto next;
			if (found_ext[ext_len] && (found_ext[ext_len]!=';')) goto next;
#endif
		}

#if defined(WIN32)
		file_info.hidden = (FindData.dwFileAttributes & FILE_ATTRIBUTE_HIDDEN) ? GF_TRUE : GF_FALSE;
		file_info.system = (FindData.dwFileAttributes & FILE_ATTRIBUTE_SYSTEM) ? GF_TRUE : GF_FALSE;
		file_info.size = MAXDWORD;
		file_info.size += 1;
		file_info.size *= FindData.nFileSizeHigh;
		file_info.size += FindData.nFileSizeLow;
		file_info.last_modified = (u64) ((*(LONGLONG *) &FindData.ftLastWriteTime - TIMESPEC_TO_FILETIME_OFFSET) / 10);
#endif

#if defined (_WIN32_WCE)
		CE_WideToChar(FindData.cFileName, file);
		strcpy(item_path, _path);
		strcat(item_path, file);
#elif defined(WIN32)
		wcscpy(item_path, path);
		wcscat(item_path, FindData.cFileName);
		file = FindData.cFileName;
#else
		strcpy(item_path, path);
		strcat(item_path, the_file->d_name);
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Checking file \"%s\" for enum\n", item_path));

		if (stat( item_path, &st ) != 0) goto next;

		file_info.directory = ((st.st_mode & S_IFMT) == S_IFDIR) ? GF_TRUE : GF_FALSE;
		if (enum_directory && !file_info.directory) goto next;
		if (!enum_directory && file_info.directory) goto next;

		file_info.size = st.st_size;

		struct tm _t = * gf_gmtime(& st.st_mtime);
		file_info.last_modified = mktime(&_t);
		file_info.last_modified *= 1000000;
#if defined(__DARWIN__) || defined(__APPLE__) || defined(GPAC_CONFIG_IOS)
		file_info.last_modified += st.st_mtimespec.tv_nsec / 1000;
#elif defined(GPAC_HAS_MTIM_NSEC)
		file_info.last_modified += st.st_mtim.tv_nsec / 1000;
#elif defined(GPAC_CONFIG_ANDROID)
		file_info.last_modified += st.st_mtime_nsec / 1000;
#endif

		file = the_file->d_name;
		if (file && file[0]=='.') file_info.hidden = GF_TRUE;

		if (file_info.directory) {
			char * parent_name = strrchr(item_path, '/');
			if (!parent_name) {
				file_info.drive = GF_TRUE;
			} else {
				struct stat st_parent;
				parent_name[0] = 0;
				if (stat(item_path, &st_parent) == 0)  {
					if ((st.st_dev != st_parent.st_dev) || (st.st_ino == st_parent.st_ino) ) {
						file_info.drive = GF_TRUE;
					}
				}
				parent_name[0] = '/';
			}
		}
#endif

		Bool done = GF_FALSE;
#ifdef WIN32
		mbs_file = gf_wcs_to_utf8(file);
		mbs_item_path = gf_wcs_to_utf8(item_path);
		if (!mbs_file || !mbs_item_path)
		{
			if (mbs_file) gf_free(mbs_file);
			if (mbs_item_path) gf_free(mbs_item_path);
			return GF_IO_ERR;
		}
		if (enum_dir_fct(cbck, mbs_file, mbs_item_path, &file_info)) {
			BOOL ret = FindClose(SearchH);
			if (!ret) {
				DWORD err = GetLastError();
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] FindClose() in gf_enum_directory() returned(1) the following error code: %d\n", err));
			}
#else
		if (enum_dir_fct(cbck, file, item_path, &file_info)) {
#endif
			done = GF_TRUE;
		}

#ifdef WIN32
		gf_free(mbs_file);
		gf_free(mbs_item_path);
#endif
		if (done) break;

next:
#ifdef WIN32
		if (!FindNextFileW(SearchH, &FindData)) {
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



struct __gf_file_io
{
	u32 _reserved_null;
	void *__this;

	GF_FileIO * (*open)(GF_FileIO *fileio_ref, const char *url, const char *mode, GF_Err *out_error);
	GF_Err (*seek)(GF_FileIO *fileio, u64 offset, s32 whence);
	u32 (*read)(GF_FileIO *fileio, u8 *buffer, u32 bytes);
	u32 (*write)(GF_FileIO *fileio, u8 *buffer, u32 bytes);
	s64 (*tell)(GF_FileIO *fileio);
	Bool (*eof)(GF_FileIO *fileio);
	int (*printf)(GF_FileIO *gfio, const char *format, va_list args);
	char *(*gets)(GF_FileIO *gfio, char *ptr, u32 size);

	char *url;
	char *res_url;
	void *udta;

	u64 bytes_done, file_size_plus_one;
	Bool main_th;
	GF_FileIOCacheState cache_state;
	u32 bytes_per_sec;
	u32 write_state;

	u32 printf_alloc;
	u8* printf_buf;
};

GF_EXPORT
Bool gf_fileio_check(FILE *fp)
{
	GF_FileIO *fio = (GF_FileIO *)fp;
	if ((fp==stdin) || (fp==stderr) || (fp==stdout))
		return GF_FALSE;

	if (fio && !fio->_reserved_null && (fio->__this==fio))
		return GF_TRUE;
	return GF_FALSE;
}

GF_EXPORT
GF_FileIOWriteState gf_fileio_write_ready(FILE *fp)
{
	GF_FileIO *fio = (GF_FileIO *)fp;
	if ((fp==stdin) || (fp==stderr) || (fp==stdout))
		return GF_FIO_WRITE_READY;

	//GFIO, check state
	if (fio && !fio->_reserved_null && (fio->__this==fio)) {
		return fio->write_state;
	}
	//sync stdio
	return GF_FIO_WRITE_READY;
}

u32 gf_fileio_get_write_rate(FILE *fp)
{
	GF_FileIO *fio = (GF_FileIO *)fp;
	if ((fp==stdin) || (fp==stderr) || (fp==stdout))
		return 0;

	if (fio && !fio->_reserved_null && (fio->__this==fio)) {
		return fio->bytes_per_sec;
	}
	//sync stdio
	return 0;
}

typedef struct
{
	u8 *data;
	u32 size;
	u32 pos;
	u32 url_crc;
	u32 nb_ref;
} GF_FileIOBlob;

static GF_FileIO *gfio_blob_open(GF_FileIO *fileio_ref, const char *url, const char *mode, GF_Err *out_error)
{
	GF_FileIOBlob *blob = gf_fileio_get_udta(fileio_ref);
	if (!strcmp(mode, "close") || !strcmp(mode, "unref")) {
		if (blob->nb_ref) blob->nb_ref--;
		blob->pos = 0;
		if (blob->nb_ref)
			return NULL;

		gf_free(blob);
		gf_fileio_del(fileio_ref);
		return NULL;
	}
	if (!strcmp(mode, "ref")) {
		blob->nb_ref++;
		*out_error = GF_OK;
		return NULL;
	}
	if (!strcmp(mode, "url")) {
		*out_error = GF_BAD_PARAM;
		return NULL;
	}
	if (!strcmp(mode, "probe")) {
		u32 crc = gf_crc_32(url, (u32) strlen(url) );
		*out_error = (crc==blob->url_crc) ? GF_OK : GF_URL_ERROR;
		return NULL;
	}
	if (mode[0]!='r') {
		*out_error = GF_BAD_PARAM;
		return NULL;
	}
	blob->nb_ref++;
	if (blob->nb_ref>2) {
		*out_error = GF_BAD_PARAM;
		return NULL;
	}
	*out_error = GF_OK;
	return fileio_ref;
}

static GF_Err gfio_blob_seek(GF_FileIO *fileio, u64 offset, s32 whence)
{
	GF_FileIOBlob *blob = gf_fileio_get_udta(fileio);
	if (whence==SEEK_END) blob->pos = blob->size;
	else if (whence==SEEK_SET) blob->pos = (u32) offset;
	else {
		if (blob->pos + offset > blob->size) return GF_BAD_PARAM;
		blob->pos += (u32) offset;
	}
	return GF_OK;
}
static u32 gfio_blob_read(GF_FileIO *fileio, u8 *buffer, u32 bytes)
{
	GF_FileIOBlob *blob = gf_fileio_get_udta(fileio);
	if (bytes + blob->pos > blob->size)
		bytes = blob->size - blob->pos;
	if (bytes) {
		memcpy(buffer, blob->data+blob->pos, bytes);
		blob->pos += bytes;
	}
	return bytes;
}
static s64 gfio_blob_tell(GF_FileIO *fileio)
{
	GF_FileIOBlob *blob = gf_fileio_get_udta(fileio);
	return (s64) blob->pos;
}
static Bool gfio_blob_eof(GF_FileIO *fileio)
{
	GF_FileIOBlob *blob = gf_fileio_get_udta(fileio);
	if (blob->pos==blob->size) return GF_TRUE;
	return GF_FALSE;
}

static char *gfio_blob_gets(GF_FileIO *fileio, char *ptr, u32 size)
{
	GF_FileIOBlob *blob = gf_fileio_get_udta(fileio);
	char *buf = blob->data + blob->pos;
	u32 len = blob->size - blob->pos;
	if (!len) return NULL;

	char *next = memchr(buf, '\n', len);
	if (next) {
		len = (u32) (next - buf);
		if (len + blob->pos<blob->size) len++;
	}
	if (len > size) len = size;
	memcpy(ptr, blob->data+blob->pos, len);
	blob->pos += len;
	return ptr;
}

GF_EXPORT
GF_FileIO *gf_fileio_new(char *url, void *udta,
	gfio_open_proc open,
	gfio_seek_proc seek,
	gfio_read_proc read,
	gfio_write_proc write,
	gfio_tell_proc tell,
	gfio_eof_proc eof,
	gfio_printf_proc printf)
{
	char szURL[100];
	GF_FileIO *tmp;

	if (!open || !seek || !tell || !eof) return NULL;

	if (!write && !read) return NULL;
	GF_SAFEALLOC(tmp, GF_FileIO);
	if (!tmp) return NULL;

	tmp->open = open;
	tmp->seek = seek;
	tmp->write = write;
	tmp->read = read;
	tmp->tell = tell;
	tmp->eof = eof;
	tmp->printf = printf;

	tmp->udta = udta;
	if (url)
		tmp->res_url = gf_strdup(url);

	sprintf(szURL, "gfio://%p", tmp);
	tmp->url = gf_strdup(szURL);
	tmp->__this = tmp;
	return tmp;
}

GF_EXPORT
const char * gf_fileio_url(GF_FileIO *gfio)
{
	return gfio ? gfio->url : NULL;
}

GF_EXPORT
const char * gf_fileio_resource_url(GF_FileIO *gfio)
{
	return gfio ? gfio->res_url : NULL;
}

GF_EXPORT
void gf_fileio_del(GF_FileIO *gfio)
{
	if (!gfio) return;
	if (gfio->url) gf_free(gfio->url);
	if (gfio->res_url) gf_free(gfio->res_url);
	if (gfio->printf_buf) gf_free(gfio->printf_buf);
	gf_free(gfio);
}

GF_EXPORT
void *gf_fileio_get_udta(GF_FileIO *gfio)
{
	return gfio ? gfio->udta : NULL;
}

GF_EXPORT
GF_FileIO *gf_fileio_open_url(GF_FileIO *gfio_ref, const char *url, const char *mode, GF_Err *out_err)
{
	if (!gfio_ref) {
		*out_err = GF_BAD_PARAM;
		return NULL;
	}
	if (!gfio_ref->open) {
		*out_err = url ? GF_NOT_SUPPORTED : GF_OK;
		return NULL;
	}
	return gfio_ref->open(gfio_ref, url, mode, out_err);
}

static u32 gf_fileio_read(GF_FileIO *gfio, u8 *buffer, u32 nb_bytes)
{
	if (!gfio) return GF_BAD_PARAM;
	if (gfio->read)
		return gfio->read(gfio, buffer, nb_bytes);
	return 0;
}

static u32 gf_fileio_write(GF_FileIO *gfio, u8 *buffer, u32 nb_bytes)
{
	if (!gfio) return GF_BAD_PARAM;
	if (!gfio->write) return 0;
	return gfio->write(gfio, buffer, (u32) nb_bytes);
}

int gf_fileio_printf(GF_FileIO *gfio, const char *format, va_list args)
{
	va_list args_copy;
	if (!gfio) return -1;
	if (gfio->printf) return gfio->printf(gfio, format, args);

	if (!gfio->write) return -1;

	va_copy(args_copy, args);
	u32 len=vsnprintf(NULL, 0, format, args_copy);
	va_end(args_copy);

	if (len>=gfio->printf_alloc) {
		gfio->printf_alloc = len+1;
		gfio->printf_buf = gf_realloc(gfio->printf_buf, gfio->printf_alloc);
	}
	vsnprintf(gfio->printf_buf, len, format, args);
	gfio->printf_buf[len] = 0;
	return gfio->write(gfio, gfio->printf_buf, len+1);
}

GF_EXPORT
Bool gf_fileio_write_mode(GF_FileIO *gfio)
{
	return (gfio && gfio->write) ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
Bool gf_fileio_read_mode(GF_FileIO *gfio)
{
	return (gfio && gfio->read) ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
GF_FileIO *gf_fileio_from_url(const char *url)
{
	char szURL[100];
	GF_FileIO *ptr=NULL;
	if (!url) return NULL;
	if (strncmp(url, "gfio://", 7)) return NULL;

	sscanf(url, "gfio://%p", &ptr);
	sprintf(szURL, "gfio://%p", ptr);
	if (strcmp(url, szURL))
		return NULL;

	if (ptr && ptr->url && !strcmp(ptr->url, url) ) {
		return ptr;
	}
	return NULL;
}

GF_EXPORT
const char * gf_fileio_translate_url(const char *url)
{
	GF_FileIO *gfio = gf_fileio_from_url(url);
	return gfio ? gfio->res_url : NULL;
}

Bool gf_fileio_eof(GF_FileIO *fileio)
{
	if (!fileio || !fileio->eof) return GF_TRUE;
	return fileio->eof(fileio);
}

int gf_fileio_flush(GF_FileIO *fileio)
{
	if (!fileio || !fileio->write) return 0;
	fileio->write(fileio, NULL, 0);
	return 0;
}

GF_EXPORT
const char *gf_fileio_factory(GF_FileIO *gfio, const char *new_res_url)
{
	GF_Err e;
	if (!gfio || !gfio->open) return NULL;
	GF_FileIO *new_res = gfio->open(gfio, new_res_url, "url", &e);
	if (e) return NULL;
	return gf_fileio_url(new_res);
}

GF_EXPORT
void gf_fileio_set_stats(GF_FileIO *gfio, u64 bytes_done, u64 file_size, GF_FileIOCacheState cache_state, u32 bytes_per_sec)
{
	if (!gfio) return;
	gfio->bytes_done = bytes_done;
	gfio->file_size_plus_one = file_size ? (1 + file_size) : 0;
	gfio->cache_state = cache_state;
	gfio->bytes_per_sec = bytes_per_sec;
}

GF_EXPORT
void gf_fileio_set_write_state(GF_FileIO *gfio, GF_FileIOWriteState write_state)
{
	if (!gfio) return;
	gfio->write_state = write_state;
}

GF_EXPORT
Bool gf_fileio_get_stats(GF_FileIO *gfio, u64 *bytes_done, u64 *file_size, GF_FileIOCacheState *cache_state, u32 *bytes_per_sec)
{
	if (!gf_fileio_check((FILE *)gfio))
		return GF_FALSE;

	if (bytes_done) *bytes_done = gfio->bytes_done;
	if (file_size) *file_size = gfio->file_size_plus_one ? gfio->file_size_plus_one-1 : 0;
	if (cache_state) *cache_state = gfio->cache_state;
	if (bytes_per_sec) *bytes_per_sec = gfio->bytes_per_sec;
	return GF_TRUE;
}

GF_EXPORT
GF_Err gf_fileio_tag_main_thread(GF_FileIO *fileio)
{
	if (!fileio) return GF_BAD_PARAM;
	fileio->main_th = GF_TRUE;
	return GF_OK;
}

GF_EXPORT
Bool gf_fileio_is_main_thread(const char *url)
{
	GF_FileIO *gfio = gf_fileio_from_url(url);
	if (!gfio) return GF_FALSE;
	return gfio->main_th;
}



GF_EXPORT
u64 gf_ftell(FILE *fp)
{
	if (!fp) return -1;

	if (gf_fileio_check(fp)) {
		GF_FileIO *gfio = (GF_FileIO *)fp;
		if (!gfio->tell) return -1;
		return gfio->tell(gfio);
	}

#if defined(_WIN32_WCE)
	return (u64) ftell(fp);
#elif defined(GPAC_CONFIG_WIN32) && (defined(__CYGWIN__) || defined(__MINGW32__))
#if (_FILE_OFFSET_BITS >= 64)
	return (u64) ftello64(fp);
#else
	return (u64) ftell(fp);
#endif
#elif defined(WIN32)
	return (u64) _ftelli64(fp);
#elif defined(GPAC_CONFIG_LINUX) && !defined(GPAC_CONFIG_ANDROID)
	return (u64) ftello64(fp);
#elif (defined(GPAC_CONFIG_FREEBSD) || defined(GPAC_CONFIG_DARWIN))
	return (u64) ftello(fp);
#else
	return (u64) ftell(fp);
#endif
}

GF_EXPORT
s32 gf_fseek(FILE *fp, s64 offset, s32 whence)
{
	if (!fp) return -1;
	if (gf_fileio_check(fp)) {
		GF_FileIO *gfio = (GF_FileIO *)fp;
		if (gfio->seek) {
			GF_Err e = gfio->seek(gfio, offset, whence);
			if (e) return -1;
			return 0;
		}
		return -1;
	}

#if defined(_WIN32_WCE)
	return (u64) fseek(fp, (s32) offset, whence);
#elif defined(GPAC_CONFIG_WIN32) && defined(__CYGWIN__)	/* mingw or cygwin */
#if (_FILE_OFFSET_BITS >= 64)
	return (u64) fseeko64(fp, offset, whence);
#else
	return (u64) fseek(fp, (s32) offset, whence);
#endif
#elif defined(WIN32)
	return (u64) _fseeki64(fp, offset, whence);
#elif defined(GPAC_CONFIG_LINUX) && !defined(GPAC_CONFIG_ANDROID)
	return fseeko64(fp, (off64_t) offset, whence);
#elif (defined(GPAC_CONFIG_FREEBSD) || defined(GPAC_CONFIG_DARWIN))
	return fseeko(fp, (off_t) offset, whence);
#else
	return fseek(fp, (s32) offset, whence);
#endif
}


static GF_FileIO *gf_fileio_from_blob(const char *file_name)
{
	u8 *blob_data;
	u32 blob_size, flags;
	GF_FileIOBlob *gfio_blob;
	GF_Err e = gf_blob_get(file_name, &blob_data, &blob_size, &flags);
	if (e || !blob_data) return NULL;
    gf_blob_release(file_name);

    if (flags) {
        GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Attempt at creating a GFIO object on blob corrupted or in transfer, not supported !"));
        return NULL;
    }

	GF_SAFEALLOC(gfio_blob, GF_FileIOBlob);
	if (!gfio_blob) return NULL;
	gfio_blob->data = blob_data;
	gfio_blob->size = blob_size;
	GF_FileIO *res = gf_fileio_new((char *) file_name, gfio_blob, gfio_blob_open, gfio_blob_seek, gfio_blob_read, NULL, gfio_blob_tell, gfio_blob_eof, NULL);
	if (!res) return NULL;
	res->gets = gfio_blob_gets;
	if (file_name)
		gfio_blob->url_crc = gf_crc_32(file_name, (u32) strlen(file_name) );
	return res;
}

GF_FileIO *gf_fileio_from_mem(const char *URL, const u8 *data, u32 size)
{
	GF_FileIOBlob *gfio_blob;
	GF_SAFEALLOC(gfio_blob, GF_FileIOBlob);
	if (!gfio_blob) return NULL;
	gfio_blob->data = (u8 *) data;
	gfio_blob->size = size;
	GF_FileIO *res = gf_fileio_new((char *) URL, gfio_blob, gfio_blob_open, gfio_blob_seek, gfio_blob_read, NULL, gfio_blob_tell, gfio_blob_eof, NULL);
	if (!res)  {
		gf_free(gfio_blob);
		return NULL;
	}
	res->gets = gfio_blob_gets;
	if (URL)
		gfio_blob->url_crc = gf_crc_32(URL, (u32) strlen(URL) );
	gf_fopen(gf_fileio_url(res), "r");
	return res;
}


#ifdef GPAC_CONFIG_EMSCRIPTEN
static u32 mainloop_th_id = 0;
void gf_set_mainloop_thread(u32 thread_id)
{
	mainloop_th_id = thread_id;
}
#endif

GF_EXPORT
FILE *gf_fopen_ex(const char *file_name, const char *parent_name, const char *mode, Bool no_warn)
{
	FILE *res = NULL;
	u32 gfio_type = 0;
	Bool is_mkdir = GF_FALSE;

	if (!file_name || !mode) return NULL;
	if (!strcmp(mode, "mkdir")) {
		is_mkdir = GF_TRUE;
		mode = "w";
	}

	if (!strncmp(file_name, "gmem://", 7)) {
		GF_FileIO *new_gfio;
		if (strstr(mode, "w"))
			return NULL;
		new_gfio = gf_fileio_from_blob(file_name);
		if (new_gfio)
			gf_register_file_handle((char*)file_name, (FILE *) new_gfio, GF_FALSE);
		return (FILE *) new_gfio;

	}

	if (!strncmp(file_name, "gfio://", 7))
		gfio_type = 1;
	else if (parent_name && !strncmp(parent_name, "gfio://", 7))
		gfio_type = 2;

	if (gfio_type) {
		GF_FileIO *gfio_ref;
		GF_FileIO *new_gfio;
		GF_Err e;
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Open GFIO %s in mode %s\n", file_name, mode));

		if (gfio_type==1)
			gfio_ref = gf_fileio_from_url(file_name);
		else
			gfio_ref = gf_fileio_from_url(parent_name);

		if (!gfio_ref) return NULL;
		if (strchr(mode, 'r') && !gf_fileio_read_mode(gfio_ref)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("FileIO %s is not read-enabled and open mode %s was requested\n", file_name, mode));
			return NULL;
		}
		if ((strchr(mode, 'w') || strchr(mode, 'a'))  && !gf_fileio_write_mode(gfio_ref)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("FileIO %s is not write-enabled and open mode %s was requested\n", file_name, mode));
			return NULL;
		}
		new_gfio = gf_fileio_open_url(gfio_ref, file_name, mode, &e);
		if (e) {
			if (!no_warn) {
				GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("FileIO %s open in mode %s failed: %s\n", file_name, mode, gf_error_to_string(e)));
			}
			return NULL;
		}
		if (new_gfio)
			gf_register_file_handle((char*)file_name, (FILE *) new_gfio, GF_FALSE);
		return (FILE *) new_gfio;
	}

	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] Open file %s in mode %s\n", file_name, mode));
	if (strchr(mode, 'w')) {
		char *fname = gf_strdup(file_name);
		char *sep = strchr(fname, '/');
		if (!sep) sep = strchr(fname, '\\');
		if (file_name[0] == '/') sep = strchr(fname+1, '/');
		else if (file_name[2] == '\\') sep = strchr(fname+3, '\\');

		while (sep) {
			char *n_sep;
			char c = sep[0];
			sep[0] = 0;
			if (!gf_dir_exists(fname)) {
				GF_Err e = gf_mkdir(fname);
				if (e != GF_OK) {
					GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Failed to create directory \"%s\": %s\n", file_name, gf_error_to_string(e) ));
					sep[0] = c;
					gf_free(fname);
					return NULL;
				}
			}
			sep[0] = c;
			n_sep = strchr(sep+1, '/');
			if (!n_sep) n_sep = strchr(sep+1, '\\');
			sep = n_sep;
		}
		gf_free(fname);
		if (is_mkdir) return NULL;
	}

#ifdef GPAC_CONFIG_EMSCRIPTEN
	if (mainloop_th_id && strchr(mode, 'r') && (gf_th_id() != mainloop_th_id)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] Opening file in read mode outside of main loop will likely deadlock - please report to GPAC devs\n"));
	}
#endif

#if defined(WIN32)
	wchar_t *wname;
	wchar_t *wmode;

	wname = gf_utf8_to_wcs(file_name);
	wmode = gf_utf8_to_wcs(mode);
	if (!wname || !wmode)
	{
		if (wname) gf_free(wname);
		if (wmode) gf_free(wmode);
		return NULL;
	}
	res = _wfsopen(wname, wmode, _SH_DENYNO);
	gf_free(wname);
	gf_free(wmode);
#elif defined(GPAC_CONFIG_LINUX) && !defined(GPAC_CONFIG_ANDROID)
	res = fopen64(file_name, mode);
#elif (defined(GPAC_CONFIG_FREEBSD) || defined(GPAC_CONFIG_DARWIN))
	res = fopen(file_name, mode);
#else
	res = fopen(file_name, mode);
#endif

	if (res) {
		if (!parent_name || strcmp(parent_name, "__temp_file"))
			gf_register_file_handle((char*)file_name, res, GF_FALSE);

		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[Core] file \"%s\" opened in mode \"%s\" - %d file handles\n", file_name, mode, gpac_file_handles));
	} else if (!no_warn) {
		if (strchr(mode, 'w') || strchr(mode, 'a')) {
#if defined(WIN32)
			u32 err = GetLastError();
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] system failure for file opening of \"%s\" in mode \"%s\": 0x%08x\n", file_name, mode, err));
#else
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] system failure for file opening of \"%s\" in mode \"%s\": %d\n", file_name, mode, errno));
#endif
		}
	}
	return res;
}

GF_EXPORT
FILE *gf_fopen(const char *file_name, const char *mode)
{
	return gf_fopen_ex(file_name, NULL, mode, GF_FALSE);
}

GF_EXPORT
s32 gf_fclose(FILE *file)
{
	if (!file)
		return 0;

	if (gf_unregister_file_handle(file))
		return 0;
	if (gf_fileio_check(file)) {
		GF_Err e;
		gf_fileio_open_url((GF_FileIO *) file, NULL, "close", &e);
		if (e) return -1;
		return 0;
	}
	return fclose(file);
}

#if (_POSIX_C_SOURCE >= 200112L || _XOPEN_SOURCE >= 600) && ! defined(_GNU_SOURCE) && !defined(WIN32)
#define HAVE_STRERROR_R 1
#endif

GF_EXPORT
size_t gf_fwrite(const void *ptr, size_t nb_bytes, FILE *stream)
{
	size_t result=0;

	if (gf_fileio_check(stream)) {
		return(size_t) gf_fileio_write((GF_FileIO *)stream, (u8 *) ptr, (u32) nb_bytes);
	}

	if (ptr)
		result = fwrite(ptr, 1, nb_bytes, stream);
	if (result != nb_bytes) {
#ifdef _WIN32_WCE
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Error writing data: %d blocks to write but %d blocks written\n", nb_bytes, result));
#else
#if defined WIN32 && !defined(GPAC_CONFIG_WIN32)
		errno_t errno_save;
		_get_errno(&errno_save);
#else
		int errno_save = errno;
#endif
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

		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Error writing data (%s): %d blocks to write but %d blocks written\n", errstr, nb_bytes, result));
#endif
	}
	return result;
}

GF_EXPORT
size_t gf_fread(void *ptr, size_t nbytes, FILE *stream)
{
	size_t result;
	if (gf_fileio_check(stream)) {
		return (size_t) gf_fileio_read((GF_FileIO *)stream, ptr, (u32) nbytes);
	}
	result = fread(ptr, 1, nbytes, stream);
	return result;
}

GF_EXPORT
char *gf_fgets(char *ptr, size_t size, FILE *stream)
{
	if (gf_fileio_check(stream)) {
		GF_FileIO *fio = (GF_FileIO *)stream;
		if (fio->gets)
			return fio->gets(fio, ptr, (u32) size);

		u32 i, read, nb_read=0;
		for (i=0; i<size; i++) {
			u8 buf[1];
			read = (u32) gf_fileio_read(fio, buf, 1);
			if (!read) break;

			ptr[nb_read] = buf[0];
			nb_read++;
			if (buf[0]=='\n') break;
		}
		if (!nb_read) return NULL;
		return ptr;
	}
	return fgets(ptr, (u32) size, stream);
}

GF_EXPORT
int gf_fgetc(FILE *stream)
{
	if (gf_fileio_check(stream)) {
		GF_FileIO *fio = (GF_FileIO *)stream;
		u8 buf[1];
		u32 read = gf_fileio_read(fio, buf, 1);
		if (!read) return -1;
		return buf[0];
	}
	return fgetc(stream);
}

GF_EXPORT
int gf_fputc(int val, FILE *stream)
{
	if (gf_fileio_check(stream)) {
		GF_FileIO *fio = (GF_FileIO *)stream;
		u32 write;
		u8 buf[1];
		buf[0] = val;
		write = gf_fileio_write(fio, buf, 1);
		if (!write) return -1;
		return buf[0];
	}
	return fputc(val, stream);
}

GF_EXPORT
int	gf_fputs(const char *buf, FILE *stream)
{
	if (gf_fileio_check(stream)) {
		GF_FileIO *fio = (GF_FileIO *)stream;
		u32 write, len = (u32) strlen(buf);
		write = gf_fileio_write(fio, (u8 *) buf, len);
		if (write != len) return -1;
		return write;
	}
	return fputs(buf, stream);
}

GF_EXPORT
int gf_fprintf(FILE *stream, const char *format, ...)
{
	int	res;
	va_list args;
	va_start(args, format);
	if (gf_fileio_check(stream)) {
		res = gf_fileio_printf((GF_FileIO *)stream, format, args);
	} else {
		res = vfprintf(stream, format, args);
	}
	va_end(args);
	return res;
}

GF_EXPORT
int gf_vfprintf(FILE *stream, const char *format, va_list args)
{
	int	res;
	if (gf_fileio_check(stream)) {
		res = gf_fileio_printf((GF_FileIO *)stream, format, args);
	} else {
		res = vfprintf(stream, format, args);
	}
	return res;
}

GF_EXPORT
int gf_fflush(FILE *stream)
{
	if (gf_fileio_check(stream)) {
		return gf_fileio_flush((GF_FileIO *)stream);
	}
	return fflush(stream);
}

GF_EXPORT
int gf_feof(FILE *stream)
{
	if (gf_fileio_check(stream)) {
		return gf_fileio_eof((GF_FileIO *)stream) ? 1 : 0;
	}
	return feof(stream);
}


GF_EXPORT
int gf_ferror(FILE *stream)
{
	if (gf_fileio_check(stream)) {
		return 0;
	}
	return ferror(stream);
}

GF_EXPORT
u64 gf_fsize(FILE *fp)
{
	u64 size;

	if (gf_fileio_check(fp)) {
		GF_FileIO *gfio = (GF_FileIO *)fp;
		if (gfio->file_size_plus_one) {
			gf_fseek(fp, 0, SEEK_SET);
			return gfio->file_size_plus_one - 1;
		}
		//fall through
	}
	gf_fseek(fp, 0, SEEK_END);
	size = gf_ftell(fp);
	gf_fseek(fp, 0, SEEK_SET);
	return size;
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
#if GF_PATH_SEPARATOR != '/'
		// windows paths can mix slashes and backslashes
		// so we search for the last slash that occurs after the last backslash
		// if it occurs before it's not relevant
		// if there's no backslashes we search in the whole file path

		char* trailingSlash = strrchr(lastPathPart?lastPathPart:filename, '/');
		if (trailingSlash)
			lastPathPart = trailingSlash;
#endif
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
	char* basename;

	if (filename && !strncmp(filename, "gfio://", 7)) {
		GF_FileIO *gfio = gf_fileio_from_url(filename);
		filename = gf_fileio_resource_url(gfio);
	}
	basename = gf_file_basename(filename);

	if (basename) {
		char *ext = strrchr(basename, '.');
		if (ext && !strcmp(ext, ".gz")) {
			ext[0] = 0;
			char *ext2 = strrchr(basename, '.');
			ext[0] = '.';
			if (ext2) return ext2;
		}
		return ext;
	}
	return NULL;
}


GF_EXPORT
char* gf_url_colon_suffix(const char *path, char assign_sep)
{
	char *sep = strchr(path, ':');
	if (!sep) return NULL;

	//handle Z:\ and Z:/
	if ((path[1]==':') && ( (path[2]=='/') || (path[2]=='\\') ) )
		return gf_url_colon_suffix(path+2, assign_sep);

	if (!strncmp(path, "gfio://", 7) || !strncmp(path, "gmem://", 7)) {
		return strchr(path+7, ':');
	}

	//handle "\\foo\Z:\bar"
	if ((path[0] == '\\') && (path[1] == '\\')) {
		char *next = strchr(path+2, '\\');
		if (next) next = strchr(next + 1, '\\');
		if (next)
			return gf_url_colon_suffix(next + 1, assign_sep);
	}


	//handle PROTO://ADD:PORT/
	if ((sep[1]=='/') && (sep[2]=='/')) {
		char *next_colon, *next_slash, *userpass;
		sep++;
		//skip all // (eg PROTO://////////////mytest/)
		while (sep[0]=='/')
			sep++;
		if (!sep[0]) return NULL;

		/*we may now have C:\ or C:/  (eg file://///C:\crazy\ or  file://///C:/crazy/)
			if sep[1]==':', then sep[2] is valid (0 or something else), no need to check for len
		*/
		if ((sep[1]==':') && ( (sep[2]=='/') || (sep[2]=='\\') ) ) {
			return gf_url_colon_suffix(sep+2, assign_sep);
		}
		//find closest : or /, if : is before / consider this is a port or an IPv6 address and check next : after /
		next_colon = strchr(sep, ':');
		next_slash = strchr(sep, '/');
		userpass = strchr(sep, '@');
		//if ':' is before '@' with '@' before next '/', consider this is `user:pass@SERVER`
		if (userpass && next_colon && next_slash && (userpass<next_slash) && (userpass>next_colon))
			next_colon = strchr(userpass, ':');

		if (next_colon && next_slash && ((next_slash - sep) > (next_colon - sep)) ) {
			const char *last_colon;
			u32 i, port, nb_colons=0, nb_dots=0, nb_non_alnums=0;
			next_slash[0] = 0;
			last_colon = strrchr(next_colon, ':');
			port = atoi(last_colon+1);
			for (i=0; i<strlen(next_colon+1); i++) {
				if (next_colon[i+1] == ':') nb_colons++;
				else if (next_colon[i+1] == '.') nb_dots++;
				//closing bracket of IPv6
				else if (next_colon[i+1] == ']') {}
				else if (!isalnum(next_colon[i+1])) {
					nb_non_alnums++;
					break;
				}
			}
			next_slash[0] = '/';
			//if no non-alphanum, we must have either a port (ipv4) or extra colons but no dots (ipv6)
			if (!nb_non_alnums && (port || (nb_colons && !nb_dots)))
				next_colon = strchr(next_slash, ':');
		}
		return next_colon;
	}

	if (sep && assign_sep) {
		char *file_ext = strchr(path, '.');
		char *assign = strchr(path, assign_sep);
		if (assign && assign>file_ext) assign = NULL;
		if (assign) file_ext = NULL;
		if (file_ext && (file_ext>sep)) {
			sep = strchr(file_ext, ':');
		}
		if (assign && (strlen(assign) > 4)) {
			if ((assign[2] == ':') && ((assign[3] == '\\') || (assign[3] == '/'))) {
				return gf_url_colon_suffix(assign + 1, 0);
			}
			if ((assign[1] == '\\') && (assign[2] == '\\')) {
				char *next = strchr(assign + 3, '\\');
				if (next) next = strchr(next+1, '\\');
				if (next && (next>sep))
					return gf_url_colon_suffix(next, 0);
			}
		}
	}
	return sep;
}
