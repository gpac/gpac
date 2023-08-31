/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2023
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

#include <gpac/config_file.h>


#if defined(WIN32) || defined(_WIN32_WCE)
#include <windows.h> /*for GetModuleFileName*/

#ifndef _WIN32_WCE
#include <direct.h>  /*for _mkdir*/
#include <gpac/utf.h>
#include <shlobj.h>  /*for getting user-dir*/

#ifndef SHGFP_TYPE_CURRENT
#define SHGFP_TYPE_CURRENT 0 /*needed for MinGW*/
#endif

#endif

#define CFG_FILE_NAME	"GPAC.cfg"
#define TEST_MODULE		"gm_"

#elif (defined(__DARWIN__) || defined(__APPLE__) )
#include <mach-o/dyld.h> /*for _NSGetExecutablePath */

#ifdef GPAC_CONFIG_IOS
#define TEST_MODULE     "gpac4ios"
#else
#define TEST_MODULE		"gm_"
#endif
#define CFG_FILE_NAME	"GPAC.cfg"

#else
#ifdef GPAC_CONFIG_LINUX
#include <unistd.h>
#endif

#define CFG_FILE_NAME	"GPAC.cfg"

#if defined(GPAC_CONFIG_WIN32)
#define TEST_MODULE		"gm_"
#else
#define TEST_MODULE		"gm_"
#endif

#endif

#if !defined(GPAC_STATIC_MODULES) && !defined(GPAC_MP4BOX_MINI)

static Bool mod_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	if (!strncmp(item_name, "gm_", 3) || !strncmp(item_name, "gf_", 3)) {
		*(Bool*)cbck = GF_TRUE;
		return GF_TRUE;
	}
	return GF_FALSE;
}
#endif

static Bool check_file_exists(char *name, char *path, char *outPath)
{
	char szPath[GF_MAX_PATH];
	FILE *f;
	int concatres;

	if (! gf_dir_exists(path)) return 0;

	if (!strcmp(name, TEST_MODULE)) {
		Bool res = GF_FALSE;
#if defined(GPAC_STATIC_MODULES) || defined(GPAC_MP4BOX_MINI)
		res = GF_TRUE;
#else
		gf_enum_directory(path, GF_FALSE, mod_enum, &res, NULL);
#endif
		if (!res) return GF_FALSE;
		if (outPath != path) strcpy(outPath, path);
		return 1;
	}

	concatres = snprintf(szPath, GF_MAX_PATH, "%s%c%s", path, GF_PATH_SEPARATOR, name);
	if (concatres<0) {
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Path too long (limit %d) when trying to concatenate %s and %s\n", GF_MAX_PATH, path, name));
	}

	//do not use gf_fopen here, we don't want to throw en error if failure
	f = fopen(szPath, "rb");
	if (!f) return GF_FALSE;
	fclose(f);
	if (outPath != path) strcpy(outPath, path);
	return GF_TRUE;
}

enum
{
	GF_PATH_APP,
	GF_PATH_CFG,
	//were we store gui/%, shaders/*, scripts/*
	GF_PATH_SHARE,
	GF_PATH_MODULES,
	GF_PATH_LIB
};

#if defined(WIN32) || defined(_WIN32_WCE)
static Bool get_default_install_path(char *file_path, u32 path_type)
{
	FILE *f;
	char szPath[GF_MAX_PATH];

#ifdef _WIN32_WCE
	TCHAR w_szPath[GF_MAX_PATH];
	GetModuleFileName(NULL, w_szPath, GF_MAX_PATH);
	CE_WideToChar((u16 *) w_szPath, file_path);
#else
	wchar_t wtmp_file_path[GF_MAX_PATH];
	char* tmp_file_path;

	GetModuleFileNameA(NULL, file_path, GF_MAX_PATH);
#endif

	/*remove exe name*/
	if (strstr(file_path, ".exe")) {
		char *sep = strrchr(file_path, '\\');
		if (sep) sep[0] = 0;
	}

	strcpy(szPath, file_path);
	strlwr(szPath);

	/*if this is run from a browser, we do not get our app path - fortunately on Windows, we always use 'GPAC' in the
	installation path*/
	if (!strstr(file_path, "gpac") && !strstr(file_path, "GPAC") ) {
		HKEY hKey = NULL;
		DWORD dwSize = GF_MAX_PATH;
		file_path[0] = 0;

		/*locate the key in current user, then in local machine*/
#ifdef _WIN32_WCE
		DWORD dwType = REG_SZ;
		u16 w_path[1024];
		RegOpenKeyEx(HKEY_CURRENT_USER, TEXT("Software\\GPAC"), 0, KEY_READ, &hKey);
#ifdef _DEBUG
		if (RegQueryValueEx(hKey, TEXT("DebugDir"), 0, &dwType, (LPBYTE) w_path, &dwSize) != ERROR_SUCCESS)
#endif
			RegQueryValueEx(hKey, TEXT("InstallDir"), 0, &dwType, (LPBYTE) w_path, &dwSize);
		CE_WideToChar(w_path, (char *)file_path);
		RegCloseKey(hKey);
#else
		if (RegOpenKeyEx(HKEY_CURRENT_USER, "Software\\GPAC", 0, KEY_READ, &hKey) != ERROR_SUCCESS)
			RegOpenKeyEx(HKEY_LOCAL_MACHINE, "Software\\GPAC", 0, KEY_READ, &hKey);

		dwSize = GF_MAX_PATH;

#ifdef _DEBUG
		if (RegQueryValueEx(hKey, "DebugDir", NULL, NULL,(unsigned char*) file_path, &dwSize) != ERROR_SUCCESS)
#endif
			RegQueryValueEx(hKey, "InstallDir", NULL, NULL,(unsigned char*) file_path, &dwSize);

		RegCloseKey(hKey);
#endif
	}
	//empty path, try DLL
	if (!file_path[0] && (path_type != GF_PATH_LIB)) {
		get_default_install_path(file_path, GF_PATH_LIB);
	}

	if (path_type==GF_PATH_APP) return GF_TRUE;

	if (path_type==GF_PATH_SHARE) {
		char *sep;
		strcat(file_path, "\\share");
		if (check_file_exists("gui\\gui.bt", file_path, file_path)) return GF_TRUE;
		sep = strstr(file_path, "\\bin\\");
		if (sep) {
			sep[0] = 0;
			strcat(file_path, "\\share");
			if (check_file_exists("gui\\gui.bt", file_path, file_path)) return GF_TRUE;
		}
		return GF_FALSE;
	}
	/*modules are stored in the GPAC directory (should be changed to GPAC/modules)*/
	if (path_type==GF_PATH_MODULES) return GF_TRUE;

	if (path_type == GF_PATH_LIB) {
		HMODULE hm=NULL;
		if (GetModuleHandleEx(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
			(LPCSTR)&get_default_install_path, &hm) == 0) {
			return 0;
		}
		if (GetModuleFileName(hm, file_path, GF_MAX_PATH) == 0) {
			return 0;
		}
		char *sep = strrchr(file_path, '\\');
		if (!sep) sep = strrchr(file_path, '/');
		if (sep) sep[0] = 0;
		return 1;
	}

	/*we are looking for the config file path - make sure it is writable*/
	assert(path_type == GF_PATH_CFG);

	strcpy(szPath, file_path);
	strcat(szPath, "\\gpaccfgtest.txt");
	//do not use gf_fopen here, we don't want to through any error if failure
	f = fopen(szPath, "wb");
	if (f != NULL) {
		fclose(f);
		gf_file_delete(szPath);
		return GF_TRUE;
	}
#ifdef _WIN32_WCE
	return 0;
#else
	/*no write access, get user home directory*/
	SHGetSpecialFolderPathW(NULL, wtmp_file_path, CSIDL_APPDATA, 1);
	tmp_file_path = gf_wcs_to_utf8(wtmp_file_path);
	strncpy(file_path, tmp_file_path, GF_MAX_PATH);
	file_path[GF_MAX_PATH-1] = 0;
	gf_free(tmp_file_path);

	if (file_path[strlen(file_path)-1] != '\\') strcat(file_path, "\\");
	strcat(file_path, "GPAC");
	/*create GPAC dir*/
	gf_mkdir(file_path);
	strcpy(szPath, file_path);
	strcat(szPath, "\\gpaccfgtest.txt");
	f = gf_fopen(szPath, "wb");
	/*COMPLETE FAILURE*/
	if (!f) return GF_FALSE;

	gf_fclose(f);
	gf_file_delete(szPath);
	return GF_TRUE;
#endif
}

#elif defined(GPAC_CONFIG_ANDROID)
static char android_app_data[512];
static char android_external_storage[512];

/*
	Called by GPAC JNI wrappers. If not set, default to app_data=/data/data/io.gpac.gpac and ext_storage=/sdcard
*/
GF_EXPORT
void gf_sys_set_android_paths(const char *app_data, const char *ext_storage)
{
	if (app_data && (strlen(app_data)<512)) {
		strcpy(android_app_data, app_data);
	}
	if (ext_storage && (strlen(ext_storage)<512)) {
		strcpy(android_external_storage, ext_storage);
	}
}


static Bool get_default_install_path(char *file_path, u32 path_type)
{
	if (!file_path) return 0;

	if (path_type==GF_PATH_APP) {
		strcpy(file_path, android_app_data[0] ? android_app_data : "/data/data/io.gpac.gpac");
		return 1;
	} else if (path_type==GF_PATH_CFG) {
		const char *res = android_external_storage[0] ? android_external_storage : getenv("EXTERNAL_STORAGE");
		if (!res) res = "/sdcard";
		strcpy(file_path, res);
		strcat(file_path, "/GPAC");
		//GPAC folder exists in external storage, use profile from this location
		if (gf_dir_exists(file_path)) {
			return 1;
		}
		//otherwise use profile in app data store
		strcpy(file_path, android_app_data[0] ? android_app_data : "/data/data/io.gpac.gpac");
		strcat(file_path, "/GPAC");
		return 1;
	} else if (path_type==GF_PATH_SHARE) {
		if (!get_default_install_path(file_path, GF_PATH_APP))
			return 0;
		strcat(file_path, "/share");
		return 1;
	} else if (path_type==GF_PATH_MODULES) {
		if (!get_default_install_path(file_path, GF_PATH_APP))
			return 0;
		strcat(file_path, "/lib");
		return 1;
	}
	return 0;
}


#elif defined(__SYMBIAN__)

#if defined(__SERIES60_3X__)
#define SYMBIAN_GPAC_CFG_DIR	"\\private\\F01F9075"
#define SYMBIAN_GPAC_GUI_DIR	"\\private\\F01F9075\\gui"
#define SYMBIAN_GPAC_MODULES_DIR	"\\sys\\bin"
#else
#define SYMBIAN_GPAC_CFG_DIR	"\\system\\apps\\GPAC"
#define SYMBIAN_GPAC_GUI_DIR	"\\system\\apps\\GPAC\\gui"
#define SYMBIAN_GPAC_MODULES_DIR	GPAC_CFG_DIR
#endif

static Bool get_default_install_path(char *file_path, u32 path_type)
{
	if (path_type==GF_PATH_APP) strcpy(file_path, SYMBIAN_GPAC_MODULES_DIR);
	else if (path_type==GF_PATH_CFG) strcpy(file_path, SYMBIAN_GPAC_CFG_DIR);
	else if (path_type==GF_PATH_GUI) strcpy(file_path, SYMBIAN_GPAC_GUI_DIR);
	else if (path_type==GF_PATH_MODULES) strcpy(file_path, SYMBIAN_GPAC_MODULES_DIR);
	return 1;
}

/*Linux, OSX, iOS*/
#else

//dlinfo
#if defined(__DARWIN__) || defined(__APPLE__)
#include <dlfcn.h>

typedef Dl_info _Dl_info;
#elif defined(GPAC_CONFIG_LINUX)


typedef struct
{
	const char *dli_fname;
	void *dli_fbase;
	const char *dli_sname;
	void *dli_saddr;
} _Dl_info;
int dladdr(void *, _Dl_info *);

#endif

static Bool get_default_install_path(char *file_path, u32 path_type)
{
	char app_path[GF_MAX_PATH];
	char *sep;
#if (defined(__DARWIN__) || defined(__APPLE__) || defined(GPAC_CONFIG_LINUX))
	u32 size;
#endif

	/*on OSX, Linux & co, user home is where we store the cfg file*/
	if (path_type==GF_PATH_CFG) {

#ifdef GPAC_CONFIG_EMSCRIPTEN
		if (gf_dir_exists("/idbfs")) {
			if (!gf_dir_exists("/idbfs/.gpac")) {
				gf_mkdir("/idbfs/.gpac");
			}
			strcpy(file_path, "/idbfs/.gpac");
			return 1;
		}
#endif

		char *user_home = getenv("HOME");
#ifdef GPAC_CONFIG_IOS
		char buf[PATH_MAX];
		char *res;
#endif

		if (!user_home) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Couldn't find HOME directory\n"));
			return 0;
		}
#ifdef GPAC_CONFIG_IOS
		res = realpath(user_home, buf);
		if (res) {
			strcpy(file_path, buf);
			strcat(file_path, "/Documents");
		} else
#endif
			strcpy(file_path, user_home);

		if (file_path[strlen(file_path)-1] == '/') file_path[strlen(file_path)-1] = 0;

		//cleanup of old install in .gpacrc
		if (check_file_exists(".gpacrc", file_path, file_path)) {
			strcpy(app_path, file_path);
			strcat(app_path, "/.gpacrc");
			gf_file_delete(app_path);
		}

		strcat(file_path, "/.gpac");
		if (!gf_dir_exists(file_path)) {
			gf_mkdir(file_path);
		}
		return 1;
	}

	if (path_type==GF_PATH_APP) {
#if (defined(__DARWIN__) || defined(__APPLE__) )
		size = GF_MAX_PATH-1;
		if (_NSGetExecutablePath(app_path, &size) ==0) {
			realpath(app_path, file_path);
			sep = strrchr(file_path, '/');
			if (sep) sep[0] = 0;
			return 1;
		}

#elif defined(GPAC_CONFIG_LINUX)
		size = readlink("/proc/self/exe", file_path, GF_MAX_PATH-1);
		if (size>0) {
			file_path[size] = 0;
			sep = strrchr(file_path, '/');
			if (sep) sep[0] = 0;
			return 1;
		}

#elif defined(GPAC_CONFIG_WIN32)
		GetModuleFileNameA(NULL, file_path, GF_MAX_PATH);
		if (strstr(file_path, ".exe")) {
			sep = strrchr(file_path, '\\');
			if (sep) sep[0] = 0;
			if ((file_path[1]==':') && (file_path[2]=='\\')) {
				strcpy(file_path, &file_path[2]);
			}
			sep = file_path;
			while ( sep[0] ) {
				if (sep[0]=='\\') sep[0]='/';
				sep++;
			}
			//get rid of /mingw32 or /mingw64
			sep = strstr(file_path, "/usr/");
			if (sep) {
				strcpy(file_path, sep);
			}
			return 1;
		}
#elif defined(GPAC_CONFIG_EMSCRIPTEN)
		return 0;
#endif
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unknown arch, cannot find executable path\n"));
		return 0;
	}

	if (path_type==GF_PATH_LIB) {
#if defined(__DARWIN__) || defined(__APPLE__) || defined(GPAC_CONFIG_LINUX)
		_Dl_info dl_info;
		dl_info.dli_fname = NULL;
		if (dladdr((void *)get_default_install_path, &dl_info)
			&& dl_info.dli_fname
		) {
			strcpy(file_path, dl_info.dli_fname);
			sep = strrchr(file_path, '/');
			if (sep) sep[0] = 0;
			return 1;
		}
		return 0;
#endif

		//for emscripten we use a static load for now
#if !defined(GPAC_CONFIG_EMSCRIPTEN)
		GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Unknown arch, cannot find library path\n"));
#endif
		return 0;
	}

#if defined(GPAC_CONFIG_EMSCRIPTEN)
	strcpy(app_path, "/usr/");
#else
	/*locate the app*/
	if (!get_default_install_path(app_path, GF_PATH_APP)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Couldn't find GPAC binaries install directory\n"));
		return 0;
	}
#endif

	/*installed or symlink on system, user user home directory*/
	if (!strnicmp(app_path, "/usr/", 5) || !strnicmp(app_path, "/opt/", 5)) {
		if (path_type==GF_PATH_SHARE) {
			/*look in possible install dirs ...*/
			if (check_file_exists("gui/gui.bt", "/usr/share/gpac", file_path)) return 1;
			if (check_file_exists("gui/gui.bt", "/usr/local/share/gpac", file_path)) return 1;
			if (check_file_exists("gui/gui.bt", "/opt/share/gpac", file_path)) return 1;
			if (check_file_exists("gui/gui.bt", "/opt/local/share/gpac", file_path)) return 1;
		} else if (path_type==GF_PATH_MODULES) {
			/*look in possible install dirs ...*/
			if (check_file_exists(TEST_MODULE, "/usr/lib64/gpac", file_path)) return 1;
			if (check_file_exists(TEST_MODULE, "/usr/lib/gpac", file_path)) return 1;
			if (check_file_exists(TEST_MODULE, "/usr/local/lib/gpac", file_path)) return 1;
			if (check_file_exists(TEST_MODULE, "/opt/lib/gpac", file_path)) return 1;
			if (check_file_exists(TEST_MODULE, "/opt/local/lib/gpac", file_path)) return 1;
			if (check_file_exists(TEST_MODULE, "/usr/lib/x86_64-linux-gnu/gpac", file_path)) return 1;
			if (check_file_exists(TEST_MODULE, "/usr/lib/i386-linux-gnu/gpac", file_path)) return 1;
		}
	}

	if (path_type==GF_PATH_SHARE) {
		Bool try_lib=GF_TRUE;
		if (get_default_install_path(app_path, GF_PATH_CFG)) {
			sep = strstr(app_path, ".gpac/");
			if (sep) sep[5]=0;
			/*GUI not found, look in ~/.gpac/share/gui/ */
			strcat(app_path, "/share");
			if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
		}

		/*GUI not found, look in gpac distribution if any */
		if (get_default_install_path(app_path, GF_PATH_APP)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[core] trying to locate share from application dir %s\n", app_path));
			strcat(app_path, "/");
retry_lib:
			sep = strstr(app_path, "/bin/");
			if (sep) {
				sep[0] = 0;
				strcat(app_path, "/share");
				if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
				strcat(app_path, "/gpac");
				if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
			}
			sep = strstr(app_path, "/build/");
			if (sep) {
				sep[0] = 0;
				strcat(app_path, "/share");
				if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
			}
		}
		if (get_default_install_path(app_path, GF_PATH_LIB)) {
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[core] trying to locate share from dynamic libgpac dir %s\n", app_path));
			sep = strstr(app_path, "/lib");
			if (sep) {
				sep[0] = 0;
				strcat(app_path, "/share");
				if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
			}
			if (try_lib) {
				try_lib = GF_FALSE;
				goto retry_lib;
			}
		}
		/*GUI not found, look in .app for OSX case*/
	}

	if (path_type==GF_PATH_MODULES) {
		/*look in gpac compilation tree (modules are output in the same folder as apps) and in distrib tree */
		if (get_default_install_path(app_path, GF_PATH_APP)) {
			if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;

			/*on OSX check modules subdirectory */
			strcat(app_path, "/modules");
			if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;

			get_default_install_path(app_path, GF_PATH_APP);
			strcat(app_path, "/");
			sep = strstr(app_path, "/bin/");
			if (sep) {
				sep[0] = 0;
				strcat(app_path, "/lib/gpac");
				if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;
			}

			/*modules not found*/
			GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Couldn't find any modules in standard path (app path %s)\n", app_path));
		}

		/*look in lib install */
		if (get_default_install_path(app_path, GF_PATH_LIB)) {
			if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;
			strcat(app_path, "/gpac");
			if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Couldn't find any modules in lib path %s\n", app_path));
		}


		/*modules not found, look in ~/.gpac/modules/ */
		if (get_default_install_path(app_path, GF_PATH_CFG)) {
			strcat(app_path, "/modules");
			if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;
		}
		/*modules not found, failure*/
#ifndef GPAC_STATIC_MODULES
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Couldn't find any modules in HOME path (app path %s)\n", app_path));
#endif
		return 0;
	}

	/*OSX way vs iPhone*/
	sep = strstr(app_path, ".app/");
	if (sep) sep[4] = 0;

	/*we are looking for .app install path, or GUI */
	if (path_type==GF_PATH_SHARE) {
#ifndef GPAC_CONFIG_IOS
		strcat(app_path, "/Contents/MacOS/share");
		if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
#else /*iOS: for now, everything is set flat within the package*/
		/*iOS app is distributed with embedded GUI*/
		get_default_install_path(app_path, GF_PATH_APP);
		if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
		strcat(app_path, "/share");
		if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
#endif
	}
	else { // (path_type==GF_PATH_MODULES)
		strcat(app_path, "/Contents/MacOS/modules");
		if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;
	}
	/*not found ...*/
	return 0;
}

#endif

//get real path where the .gpac dir has been created, and use this as the default path
//for cache (tmp/ dir of ios app) and last working fir
#ifdef GPAC_CONFIG_IOS
static void gf_ios_refresh_cache_directory( GF_Config *cfg, const char *file_path)
{
	char *cache_dir, *old_cache_dir;
	char buf[GF_MAX_PATH], *res, *sep;
	res = realpath(file_path, buf);
	if (!res) return;

	sep = strstr(res, ".gpac");
	assert(sep);
	sep[0] = 0;
	gf_cfg_set_key(cfg, "core", "docs-dir", res);
	if (!gf_cfg_get_key(cfg, "core", "last-dir"))
		gf_cfg_set_key(cfg, "core", "last-dir", res);

	strcat(res, "cache/");
	cache_dir = res;
	old_cache_dir = (char*) gf_opts_get_key("core", "cache");

	if (!gf_dir_exists(cache_dir)) {
		if (old_cache_dir && strcmp(old_cache_dir, cache_dir)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Cache dir changed: old %d -> new %s\n\n", old_cache_dir, cache_dir ));
		}
		gf_mkdir(cache_dir);
	}
	gf_cfg_set_key(cfg, "core", "cache", cache_dir);
}

#endif

const char * gf_get_default_cache_directory_ex(Bool do_create);

GF_EXPORT
void gf_get_default_font_dir(char szPath[GF_MAX_PATH])
{
#if defined(_WIN32_WCE)
	strcpy(szPath, "\\Windows");

#elif defined(WIN32)
	GetWindowsDirectory((char*)szPath, MAX_PATH);
	if (szPath[strlen((char*)szPath)-1] != '\\') strcat((char*)szPath, "\\");
	strcat((char *)szPath, "Fonts");

#elif defined(__APPLE__) && defined(GPAC_CONFIG_IOS)
	strcpy(szPath, "/System/Library/Fonts/Cache,/System/Library/Fonts/AppFonts,/System/Library/Fonts/Core,/System/Library/Fonts/Extra");
#elif defined(__APPLE__)
	strcpy(szPath, "/System/Library/Fonts,/Library/Fonts");

#elif defined(GPAC_CONFIG_ANDROID)
	strcpy(szPath, "/system/fonts/");
#else
	//scan all /usr/share/fonts, not just /usr/share/fonts/truetype/ which does not exist in some distrros
	strcpy(szPath, "/usr/share/fonts/");
#endif
}


static GF_Config *create_default_config(char *file_path, const char *profile)
{
	Bool moddir_found;
	GF_Config *cfg;
	char szPath[GF_MAX_PATH];

	if (file_path && ! get_default_install_path(file_path, GF_PATH_CFG)) {
		profile = "0";
	}
	/*Create temp config file*/
	if (profile && (!strcmp(profile, "0") || !stricmp(profile, "n"))) {
		cfg = gf_cfg_new(NULL, NULL);
		if (!stricmp(profile, "n")) {
			gf_cfg_discard_changes(cfg);
			return cfg;
		}
	} else {
		FILE *f;

		/*create config file from disk*/
		if (profile) {
			sprintf(szPath, "%s%cprofiles%c%s%c%s", file_path, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR, profile, GF_PATH_SEPARATOR, CFG_FILE_NAME);
		} else {
			sprintf(szPath, "%s%c%s", file_path, GF_PATH_SEPARATOR, CFG_FILE_NAME);
		}
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Trying to create config file: %s\n", szPath ));

		f = gf_fopen(szPath, "wt");
		if (!f) return NULL;
		gf_fclose(f);

		cfg = gf_cfg_new(NULL, szPath);
	}

	if (!cfg) return NULL;


#ifndef GPAC_CONFIG_IOS
	moddir_found = get_default_install_path(szPath, GF_PATH_MODULES);
#else
	moddir_found = get_default_install_path(szPath, GF_PATH_APP);
#endif

#if defined(GPAC_CONFIG_IOS)
	gf_cfg_set_key(cfg, "core", "devclass", "ios");
#elif defined(GPAC_CONFIG_ANDROID)
	gf_cfg_set_key(cfg, "core", "devclass", "android");
#elif defined(GPAC_CONFIG_EMSCRIPTEN)
	gf_cfg_set_key(cfg, "core", "devclass", "wasm");
#else
	gf_cfg_set_key(cfg, "core", "devclass", "desktop");
#endif


	if (!moddir_found) {
#if !defined(GPAC_CONFIG_EMSCRIPTEN)
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[Core] default modules directory not found\n"));
#endif
	} else {
		gf_cfg_set_key(cfg, "core", "module-dir", szPath);
	}


#if defined(GPAC_CONFIG_IOS)
	gf_ios_refresh_cache_directory(cfg, file_path);
#elif defined(GPAC_CONFIG_ANDROID)
	if (get_default_install_path(szPath, GF_PATH_APP)) {
		strcat(szPath, "/cache");
		gf_cfg_set_key(cfg, "core", "cache", szPath);
		strcat(szPath, "/.nomedia");
		//create .nomedia in cache and in app dir
		if (!gf_file_exists(szPath)) {
			FILE *f = gf_fopen(szPath, "w");
			if (f) gf_fclose(f);
		}
		get_default_install_path(szPath, GF_PATH_APP);
		strcat(szPath, "/.nomedia");
		if (!gf_file_exists(szPath)) {
			FILE *f = gf_fopen(szPath, "w");
			if (f) gf_fclose(f);
		}
		//add a tmp as well, tmpfile() does not work on android
		get_default_install_path(szPath, GF_PATH_APP);
		strcat(szPath, "/gpac_tmp");
		gf_cfg_set_key(cfg, "core", "tmp", szPath);
		strcat(szPath, "/.nomedia");
		if (!gf_file_exists(szPath)) {
			FILE *f = gf_fopen(szPath, "w");
			if (f) gf_fclose(f);
		}
	}
#else
	/*get default temporary directoy */
	gf_cfg_set_key(cfg, "core", "cache", gf_get_default_cache_directory_ex(GF_FALSE));
#endif

	/*Setup font engine to FreeType by default, and locate TrueType font directory on the system*/
	gf_cfg_set_key(cfg, "core", "font-reader", "freetype");
	gf_cfg_set_key(cfg, "core", "rescan-fonts", "yes");


	gf_get_default_font_dir(szPath);
	gf_cfg_set_key(cfg, "core", "font-dirs", szPath);

	gf_cfg_set_key(cfg, "core", "cache-size", "100M");

#if defined(_WIN32_WCE)
	gf_cfg_set_key(cfg, "core", "video-output", "gapi");
#elif defined(WIN32)
	gf_cfg_set_key(cfg, "core", "video-output", "directx");
#elif defined(__DARWIN__) || defined(__APPLE__)
	gf_cfg_set_key(cfg, "core", "video-output", "sdl");
#elif defined(GPAC_CONFIG_ANDROID)
	gf_cfg_set_key(cfg, "core", "video-output", "android");
	gf_cfg_set_key(cfg, "core", "audio-output", "android");
#else
	//use SDL by default, will fallback to X11 if not found (our X11 wrapper is old and does not have all features of the SDL one)
	gf_cfg_set_key(cfg, "core", "video-output", "sdl");
	gf_cfg_set_key(cfg, "core", "audio-output", "sdla");
#endif


#if !defined(GPAC_CONFIG_IOS) && !defined(GPAC_CONFIG_ANDROID)
	gf_cfg_set_key(cfg, "core", "switch-vres", "no");
	gf_cfg_set_key(cfg, "core", "hwvmem", "auto");
#endif

#ifdef GPAC_CONFIG_ANDROID
	const char *opt = android_external_storage[0] ? android_external_storage : getenv("EXTERNAL_STORAGE");
	if (!opt) opt = "/sdcard";
	gf_cfg_set_key(cfg, "core", "docs-dir", opt);
	gf_cfg_set_key(cfg, "core", "last-dir", opt);
#endif

	/*locate GUI*/
	if ( get_default_install_path(szPath, GF_PATH_SHARE) ) {
		char gui_path[GF_MAX_PATH+100];
		sprintf(gui_path, "%s%cgui%cgui.bt", szPath, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		if (gf_file_exists(gui_path)) {
			gf_cfg_set_key(cfg, "core", "startup-file", gui_path);
		}

		/*shaders are at the same location*/
		sprintf(gui_path, "%s%cshaders%cvertex.glsl", szPath, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		gf_cfg_set_key(cfg, "filter@compositor", "vertshader", gui_path);
		sprintf(gui_path, "%s%cshaders%cfragment.glsl", szPath, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		gf_cfg_set_key(cfg, "filter@compositor", "fragshader", gui_path);

		//aliases and other defaults
		sprintf(gui_path, "%s%cdefault.cfg", szPath, GF_PATH_SEPARATOR);
		if (gf_file_exists(gui_path)) {
			GF_Config *aliases = gf_cfg_new(NULL, gui_path);
			if (aliases) {
				u32 i, count = gf_cfg_get_section_count(aliases);
				for (i=0; i<count; i++) {
					u32 j, count2;
					const char *sec_name = gf_cfg_get_section_name(aliases, i);
					if (!sec_name) continue;
					count2 = gf_cfg_get_key_count(aliases, sec_name);
					for (j=0; j<count2; j++) {
						const char *name = gf_cfg_get_key_name(aliases, sec_name, j);
						const char *val = gf_cfg_get_key(aliases, sec_name, name);
						gf_cfg_set_key(cfg, sec_name, name, val);
					}
				}
			}
			gf_cfg_del(aliases);
		}
	}

	if (profile && !strcmp(profile, "0")) {
		GF_Err gf_cfg_set_filename(GF_Config *iniFile, const char * fileName);
//		sprintf(szPath, "%s%c%s", gf_get_default_cache_directory(), GF_PATH_SEPARATOR, CFG_FILE_NAME);
		gf_cfg_set_filename(cfg, CFG_FILE_NAME);
		gf_cfg_discard_changes(cfg);
		return cfg;
	}
	/*store and reload*/
	strcpy(szPath, gf_cfg_get_filename(cfg));
	gf_cfg_del(cfg);
	return gf_cfg_new(NULL, szPath);
}

/*check if modules directory has changed in the config file
*/
static void check_modules_dir(GF_Config *cfg)
{
	char path[GF_MAX_PATH];

#ifdef GPAC_CONFIG_IOS
	const char *cfg_path;
	if ( get_default_install_path(path, GF_PATH_SHARE) ) {
		char *sep;
		char shader_path[GF_MAX_PATH];
		strcat(path, "/gui/gui.bt");
		gf_cfg_set_key(cfg, "core", "startup-file", path);
		//get rid of "/gui/gui.bt"
		sep = strrchr(path, '/');
		sep[0] = 0;
		sep = strrchr(path, '/');
		sep[0] = 0;

		sprintf(shader_path, "%s%cshaders%cvertex.glsl", path, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		gf_cfg_set_key(cfg, "filter@compositor", "vertshader", shader_path);
		sprintf(shader_path, "%s%cshaders%cfragment.glsl", path, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		gf_cfg_set_key(cfg, "filter@compositor", "fragshader", shader_path);
	}
	cfg_path = gf_cfg_get_filename(cfg);
	gf_ios_refresh_cache_directory(cfg, cfg_path);

#else
	const char *opt;

	if ( get_default_install_path(path, GF_PATH_MODULES) ) {
		opt = gf_cfg_get_key(cfg, "core", "module-dir");
		//for OSX, we can have an install in /usr/... and an install in /Applications/GPAC.app - always change
#if defined(__DARWIN__) || defined(__APPLE__)
		if (!opt || strcmp(opt, path))
			gf_cfg_set_key(cfg, "core", "module-dir", path);
#else

		//otherwise only check we didn't switch between a 64 bit version and a 32 bit version
		if (!opt) {
			gf_cfg_set_key(cfg, "core", "module-dir", path);
		} else  {
			Bool erase_modules_dir = GF_FALSE;
			const char *opt64 = gf_cfg_get_key(cfg, "core", "64bits");
			if (!opt64) {
				//first run or old versions, erase
				erase_modules_dir = GF_TRUE;
			} else if (!strcmp(opt64, "yes") ) {
#ifndef GPAC_64_BITS
				erase_modules_dir = GF_TRUE;
#endif
			} else {
#ifdef GPAC_64_BITS
				erase_modules_dir = GF_TRUE;
#endif
			}

#ifdef GPAC_64_BITS
			opt64 = "yes";
#else
			opt64 = "no";
#endif
			gf_cfg_set_key(cfg, "core", "64bits", opt64);

			if (erase_modules_dir) {
				gf_cfg_set_key(cfg, "core", "module-dir", path);
			}
		}
#endif
	}

	/*if startup file was disabled, do not attempt to correct it*/
	if (gf_cfg_get_key(cfg, "core", "startup-file")==NULL) return;

	if ( get_default_install_path(path, GF_PATH_SHARE) ) {
		opt = gf_cfg_get_key(cfg, "core", "startup-file");
		if (strstr(opt, "gui.bt") && strcmp(opt, path) && strstr(path, ".app") ) {
#if defined(__DARWIN__) || defined(__APPLE__)
			strcat(path, "/gui/gui.bt");
			gf_cfg_set_key(cfg, "core", "startup-file", path);
#endif
		}
	}

#endif
}

#ifdef GPAC_CONFIG_ANDROID

static Bool delete_tmp_files(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	if (gf_file_delete(item_path) != GF_OK) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CACHE, ("[CORE] Failed to cleanup temp file %s\n", item_path));
	}
	return GF_FALSE;
}
#endif

static void check_default_cred_file(GF_Config *cfg, char szPath[GF_MAX_PATH])
{
#ifndef GPAC_CONFIG_EMSCRIPTEN
	char key[16];
	u64 v1, v2;
	const char *opt = gf_cfg_get_key(cfg, "core", "cred");
	if (opt) return;
	//when running as service, the config file path may be unknown
	if (!szPath[0] || !gf_dir_exists(szPath))
		return;
	strcat(szPath, "/creds.key");
	if (gf_file_exists(szPath)) return;

	GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[core] Creating default credential key in %s, use -cred=PATH/TO_FILE to overwrite\n", szPath));

	v1 = gf_rand(); v1<<=32; v1 |= gf_rand();
	v2 = gf_rand(); v2<<=32; v2 |= gf_rand();
	v1 |= gf_sys_clock_high_res();
#ifndef GPAC_64_BITS
	v2 |= (u64) (u32) cfg;
	v2 ^= (u64) (u32) szPath;
#else
	v2 |= (u64) cfg;
	v2 ^= (u64) szPath;
#endif
	* ( (u64*) &key[0] ) = v1;
	* ( (u64*) &key[8] ) = v2;
	FILE *crd = gf_fopen(szPath, "w");
	if (!crd) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] Failed to create credential key in %s, credentials will not be stored\n", szPath));
		return;
	}
	fwrite(key, 16, 1, crd);
	fclose(crd);
	gf_cfg_set_key(cfg, "core", "cred", szPath);
#endif //GPAC_CONFIF_EMSCRIPTEN
}

/*!
\brief configuration file initialization
 *
 * Constructs a configuration file from fileName. If fileName is NULL, the default GPAC configuration file is loaded with the
 * proper module directory, font directory and other default options. If fileName is non-NULL no configuration file is found,
 * a "light" default configuration file is created.
\param profile name or path to existing config file
\return the configuration file object, NULL if the file could not be created
 */
 #include <gpac/network.h>

static GF_Config *gf_cfg_init(const char *profile)
{
	GF_Config *cfg=NULL;
	u32 prof_len=0;
	Bool force_new_cfg=GF_FALSE;
	Bool fast_profile=GF_FALSE;
	char szPath[GF_MAX_PATH];
	char *prof_opt = NULL;

	if (profile) {
		prof_len = (u32) strlen(profile);
		prof_opt = gf_url_colon_suffix(profile, 0);
		if (prof_opt) {
			prof_len -= (u32) strlen(prof_opt);
			if (strstr(prof_opt, "reload")) force_new_cfg = GF_TRUE;
			prof_opt[0] = 0;
		}
		if (!stricmp(profile, "n")) {
			fast_profile = GF_TRUE;
			cfg = create_default_config(NULL, "n");
			goto skip_cfg;
		}
	}
	if (profile && !prof_len)
		profile = NULL;

	if (profile && (strchr(profile, '/') || strchr(profile, '\\')) ) {
		if (!gf_file_exists(profile)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] Config file %s does not exist\n", profile));
			goto exit;
		}
		cfg = gf_cfg_new(NULL, profile);
		if (!cfg) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] Failed to load existing config file %s\n", profile));
			goto exit;
		}
		if (force_new_cfg) {
			gf_cfg_del(cfg);
			cfg = create_default_config(NULL, profile);
		}
		check_modules_dir(cfg);
		goto exit;
	}

	if (!get_default_install_path(szPath, GF_PATH_CFG)) {
		if (!profile || strcmp(profile, "0")) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[core] Cannot locate global config path in application or user home directory, using temporary config file\n"));
		}
		profile="0";
		cfg = create_default_config(szPath, profile);
		goto skip_cfg;
	}

	if (profile && !strcmp(profile, "0")) {
		cfg = create_default_config(NULL, "0");
		goto skip_cfg;
	}

	if (profile) {
		strcat(szPath, "/profiles/");
		strcat(szPath, profile);
	}

	cfg = gf_cfg_new(szPath, CFG_FILE_NAME);
	//config file not compatible with old arch, check it:
	if (cfg) {
		const char *key;
		u32 nb_old_sec = gf_cfg_get_key_count(cfg, "Compositor");
		nb_old_sec += gf_cfg_get_key_count(cfg, "MimeTypes");
		nb_old_sec += gf_cfg_get_key_count(cfg, "Video");
		nb_old_sec += gf_cfg_get_key_count(cfg, "Audio");
		nb_old_sec += gf_cfg_get_key_count(cfg, "Systems");
		nb_old_sec += gf_cfg_get_key_count(cfg, "General");
		if (! gf_cfg_get_key_count(cfg, "core"))
			nb_old_sec += 1;

		//check GUI is valid, if not recreate a config
		key = gf_cfg_get_key(cfg, "core", "startup-file");
		if (key && !gf_file_exists(key))
			force_new_cfg = GF_TRUE;

		//check if reset flag is set in existing config
		if (gf_cfg_get_key(cfg, "core", "reset"))
			force_new_cfg = GF_TRUE;

		if (nb_old_sec || force_new_cfg) {
			if (nb_old_sec && (!profile || strcmp(profile, "0"))) {
				GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("[core] Incompatible config file %s found in %s - creating new file\n", CFG_FILE_NAME, szPath ));
			}
			gf_cfg_del(cfg);
			cfg = create_default_config(szPath, profile);
		} else {
			//check fonts are valid, if not reload fonts
			Bool rescan_fonts = GF_FALSE;
			key = gf_cfg_get_key_name(cfg, "FontCache", 0);
			if (!key)
				rescan_fonts = GF_TRUE;
			else {
				key = gf_cfg_get_key(cfg, "FontCache", key);
				if (key && !gf_file_exists(key))
					rescan_fonts = GF_TRUE;
			}
			if (rescan_fonts)
				gf_opts_set_key("core", "rescan-fonts", "yes");
		}
	}
	//no config file found
	else {
		if (!profile || strcmp(profile, "0")) {
			GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("[core] Config file %s not found in %s - creating new file\n", CFG_FILE_NAME, szPath ));
		}
		cfg = create_default_config(szPath, profile);
	}

skip_cfg:
	if (!cfg) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[core] Cannot create config file %s in %s directory\n", CFG_FILE_NAME, szPath));
		goto exit;
	}

#ifndef GPAC_CONFIG_IOS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("[core] Using global config file in %s directory\n", szPath));
#endif

	if (fast_profile) goto exit;

	check_modules_dir(cfg);
	if (!profile || strcmp(profile, "0"))
		check_default_cred_file(cfg, szPath);

	if (!gf_cfg_get_key(cfg, "core", "store-dir")) {
		if (profile && !strcmp(profile, "0")) {
			strcpy(szPath, gf_get_default_cache_directory_ex(GF_FALSE) );
			strcat(szPath, "/Storage");
		} else {
			char *sep;
			strcpy(szPath, gf_cfg_get_filename(cfg));
			sep = strrchr(szPath, '/');
			if (!sep) sep = strrchr(szPath, '\\');
			if (sep) sep[0] = 0;
			strcat(szPath, "/Storage");
			if (!gf_dir_exists(szPath)) gf_mkdir(szPath);
		}
		gf_cfg_set_key(cfg, "core", "store-dir", szPath);
	}

exit:
	if (prof_opt) prof_opt[0] = ':';

	//clean tmp
#ifdef GPAC_CONFIG_ANDROID
	if (cfg) {
		const char *tmp = gf_cfg_get_key(cfg, "core", "tmp");
		if (tmp && !strstr(tmp, "/gpac_tmp")) tmp=NULL;
		if (tmp) {
			gf_enum_directory(tmp, GF_FALSE, delete_tmp_files, (void*)cfg, NULL);
		}
	}
#endif

	return cfg;
}


GF_EXPORT
Bool gf_opts_default_shared_directory(char *path_buffer)
{
	return get_default_install_path(path_buffer, GF_PATH_SHARE);
}

void gf_modules_new(GF_Config *config);
void gf_modules_del();

GF_Config *gpac_global_config = NULL;

void gf_init_global_config(const char *profile)
{
	if (!gpac_global_config) {
		gpac_global_config = gf_cfg_init(profile);
		if (!gpac_global_config) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Fatal error: failed to initialize GPAC global configuration\n"));
			exit(1);
		}
		if (!profile || stricmp(profile, "n"))
			gf_modules_new(gpac_global_config);
	}
}

void gf_uninit_global_config(Bool discard_config)
{
	if (gpac_global_config) {
		if (discard_config) gf_cfg_discard_changes(gpac_global_config);
		gf_cfg_del(gpac_global_config);
		gpac_global_config = NULL;
		gf_modules_del();
	}
}

GF_Err gf_cfg_set_key_internal(GF_Config *iniFile, const char *secName, const char *keyName, const char *keyValue, Bool is_restrict);

#ifdef GPAC_ENABLE_RESTRICT
void gf_cfg_load_restrict()
{
	char szPath[GF_MAX_PATH];
	if (get_default_install_path(szPath, GF_PATH_SHARE)) {
		strcat(szPath, "/");
		strcat(szPath, "restrict.cfg");
		if (gf_file_exists(szPath)) {
			GF_Config *rcfg = gf_cfg_new(NULL, szPath);
			if (rcfg) {
				u32 i, count = gf_cfg_get_section_count(rcfg);
				for (i=0; i<count; i++) {
					u32 j, kcount;
					const char *sname = gf_cfg_get_section_name(rcfg, i);
					if (!sname) break;
					kcount = gf_cfg_get_key_count(rcfg, sname);
					for (j=0; j<kcount; j++) {
						const char *kname = gf_cfg_get_key_name(rcfg, sname, j);
						const char *kval = gf_cfg_get_key(rcfg, sname, kname);
						gf_cfg_set_key_internal(gpac_global_config, sname, kname, kval, GF_TRUE);
					}
				}
				gf_cfg_del(rcfg);
			}
		}
	}
}
#endif

GF_EXPORT
const char *gf_opts_get_key(const char *secName, const char *keyName)
{
	if (!gpac_global_config) return NULL;

	if (!strcmp(secName, "core")) {
		const char *opt = gf_cfg_get_key(gpac_global_config, "temp", keyName);
		if (opt) return opt;
	}
	return gf_cfg_get_key(gpac_global_config, secName, keyName);
}
GF_EXPORT
GF_Err gf_opts_set_key(const char *secName, const char *keyName, const char *keyValue)
{
	if (!gpac_global_config) return GF_BAD_PARAM;
	return gf_cfg_set_key(gpac_global_config, secName, keyName, keyValue);
}
GF_EXPORT
void gf_opts_del_section(const char *secName)
{
	if (!gpac_global_config) return;
	gf_cfg_del_section(gpac_global_config, secName);
}
GF_EXPORT
u32 gf_opts_get_section_count()
{
	if (!gpac_global_config) return 0;
	return gf_cfg_get_section_count(gpac_global_config);
}
GF_EXPORT
const char *gf_opts_get_section_name(u32 secIndex)
{
	if (!gpac_global_config) return NULL;
	return gf_cfg_get_section_name(gpac_global_config, secIndex);
}
GF_EXPORT
u32 gf_opts_get_key_count(const char *secName)
{
	if (!gpac_global_config) return 0;
	return gf_cfg_get_key_count(gpac_global_config, secName);
}
GF_EXPORT
const char *gf_opts_get_key_name(const char *secName, u32 keyIndex)
{
	if (!gpac_global_config) return NULL;
	return gf_cfg_get_key_name(gpac_global_config, secName, keyIndex);
}

GF_EXPORT
const char *gf_opts_get_key_restricted(const char *secName, const char *keyName)
{
	const char *res = NULL;
	const char *gf_cfg_get_key_internal(GF_Config *iniFile, const char *secName, const char *keyName, Bool restricted_only);
	if (gpac_global_config)
	 	res = gf_cfg_get_key_internal(gpac_global_config, secName, keyName, GF_TRUE);
	return res;
}

GF_EXPORT
const char *gf_opts_get_filename()
{
	return gf_cfg_get_filename(gpac_global_config);
}
GF_EXPORT
GF_Err gf_opts_discard_changes()
{
	return gf_cfg_discard_changes(gpac_global_config);
}

GF_EXPORT
GF_Err gf_opts_save()
{
	return gf_cfg_save(gpac_global_config);
}

#include <gpac/main.h>

GF_GPACArg GPAC_Args[] = {
 GF_DEF_ARG("tmp", NULL, "specify directory for temporary file creation instead of OS-default temporary file management", NULL, NULL, GF_ARG_STRING, 0),
 GF_DEF_ARG("noprog", NULL, "disable progress messages", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("quiet", NULL, "disable all messages, including errors", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("log-file", "lf", "set output log file", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("log-clock", "lc", "log time in micro sec since start time of GPAC before each log line except for `app` tool", NULL, NULL, GF_ARG_BOOL, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("log-utc", "lu", "log UTC time in ms before each log line except for `app` tool", NULL, NULL, GF_ARG_BOOL, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("logs", NULL, "set log tools and levels.  \n"
			"  \n"
			"You can independently log different tools involved in a session.  \n"
			"log_args is formatted as a colon (':') separated list of `toolX[:toolZ]@levelX`  \n"
	        "`levelX` can be one of:\n"
	        "- quiet: skip logs\n"
	        "- error: logs only error messages\n"
	        "- warning: logs error+warning messages\n"
	        "- info: logs error+warning+info messages\n"
	        "- debug: logs all messages\n"
	        "\n`toolX` can be one of:\n"
	        "- core: libgpac core\n"
	        "- mutex: log all mutex calls\n"
	        "- mem: GPAC memory tracker\n"
	        "- module: GPAC modules (av out, font engine, 2D rasterizer)\n"
	        "- filter: filter session debugging\n"
	        "- sched: filter session scheduler debugging\n"
	        "- codec: codec messages (used by encoder and decoder filters)\n"
	        "- coding: bitstream formats (audio, video, scene)\n"
	        "- container: container formats (ISO File, MPEG-2 TS, AVI, ...) and multiplexer/demultiplexer filters\n"
	        "- network: TCP/UDP sockets and TLS\n"
	        "- http: HTTP traffic\n"
	        "- cache: HTTP cache subsystem\n"
	        "- rtp: RTP traffic\n"
	        "- dash: HTTP streaming logs\n"
	        "- route: ROUTE (ATSC3) debugging\n"
	        "- media: messages from generic filters and reframer/rewriter filters\n"
	        "- parser: textual parsers (svg, xmt, bt, ...)\n"
	        "- mmio: I/O management (AV devices, file, pipes, OpenGL)\n"
	        "- audio: audio renderer/mixer/output\n"
	        "- script: script engine except console log\n"
	        "- console: script console log\n"
	        "- scene: scene graph and scene manager\n"
	        "- compose: composition engine (2D, 3D, etc)\n"
	        "- ctime: media and SMIL timing info from composition engine\n"
	        "- interact: interaction messages (UI events and triggered DOM events and VRML route)\n"
	        "- rti: run-time stats of compositor\n"
	        "- all: all tools logged - other tools can be specified afterwards.  \n"
	        "The special keyword `ncl` can be set to disable color logs.  \n"
	        "The special keyword `strict` can be set to exit at first error.  \n"
	        "\nEX -logs=all@info:dash@debug:ncl\n"
			"This moves all log to info level, dash to debug level and disable color logs"
 			, NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("proglf", NULL, "use new line at each progress messages", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("log-dual", "ld", "output to both file and stderr", NULL, NULL, GF_ARG_BOOL, GF_ARG_SUBSYS_LOG),

 GF_DEF_ARG("strict-error", "se", "exit after the first error is reported", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("store-dir", NULL, "set storage directory", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("mod-dirs", NULL, "set additional module directories as a semi-colon `;` separated list", NULL, NULL, GF_ARG_STRINGS, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("js-dirs", NULL, "set javascript directories", NULL, NULL, GF_ARG_STRINGS, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("no-js-mods", NULL, "disable javascript module loading", NULL, NULL, GF_ARG_STRINGS, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("ifce", NULL, "set default multicast interface (default is ANY), either an IP address or a device name as listed by `gpac -h net`. Prefix '+' will force using IPv6 for dual interface", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("lang", NULL, "set preferred language", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("cfg", "opt", "get or set configuration file value. The string parameter can be formatted as:\n"
	        "- `section:key=val`: set the key to a new value\n"
	        "- `section:key=null`, `section:key`: remove the key\n"
	        "- `section=null`: remove the section\n"
	        "- no argument: print the entire configuration file\n"
	        "- `section`: print the given section\n"
	        "- `section:key`: print the given `key` in `section` (section can be set to `*`)"
	        "- `*:key`: print the given `key` in all sections"
			, NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("no-save", NULL, "discard any changes made to the config file upon exit", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("version", NULL, "set to GPAC version, used to check config file refresh", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("mod-reload", NULL, "unload / reload module shared libs when no longer used", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("for-test", NULL, "disable all creation/modification dates and GPAC versions in files", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("old-arch", NULL, "enable compatibility with pre-filters versions of GPAC", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("ntp-shift", NULL, "shift NTP clock by given amount in seconds", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("devclass", NULL, "set device class\n"
 "- ios: iOS-based mobile device\n"
 "- android: Android-based mobile device\n"
 "- desktop: desktop device", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_HIDE|GF_ARG_SUBSYS_CORE),

 GF_DEF_ARG("bs-cache-size", NULL, "cache size for bitstream read and write from file (0 disable cache, slower IOs)", "512", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("no-check", NULL, "disable compliance tests for inputs (ISOBMFF for now). This will likely result in random crashes", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("unhandled-rejection", NULL, "dump unhandled promise rejections", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("startup-file", NULL, "startup file of compositor in GUI mode", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("docs-dir", NULL, "default documents directory (for GUI on iOS and Android)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("last-dir", NULL, "last working directory (for GUI)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
#ifdef GPAC_HAS_POLL
 GF_DEF_ARG("no-poll", NULL, "disable poll and use select for socket groups", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
#endif
 GF_DEF_ARG("no-tls-rcfg", NULL, "disable automatic TCP to TLS reconfiguration", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("no-fd", NULL, "use buffered IO instead of file descriptor for read/write - this can speed up operations on small files", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("no-mx", NULL, "disable all mutexes, threads and semaphores (do not use if unsure about threading used)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),

 GF_DEF_ARG("cache", NULL, "cache directory location", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("proxy-on", NULL, "enable HTTP proxy", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("proxy-name", NULL, "set HTTP proxy address", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("proxy-port", NULL, "set HTTP proxy port", "80", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("maxrate", NULL, "set max HTTP download rate in bits per sec. 0 means unlimited", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("no-cache", NULL, "disable HTTP caching", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("offline-cache", NULL, "enable offline HTTP caching (no re-validation of existing resource in cache)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("clean-cache", NULL, "indicate if HTTP cache should be clean upon launch/exit", NULL, NULL, GF_ARG_BOOL, GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("cache-size", NULL, "specify cache size in bytes", "100M", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("tcp-timeout", NULL, "time in milliseconds to wait for HTTP/RTSP connect before error", "5000", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("req-timeout", NULL, "time in milliseconds to wait on HTTP/RTSP request before error (0 disables timeout)", "10000", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("no-timeout", NULL, "ignore HTTP 1.1 timeout in keep-alive", "false", NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("broken-cert", NULL, "enable accepting broken SSL certificates", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("user-agent", "ua", "set user agent name for HTTP/RTSP", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("user-profileid", NULL, "set user profile ID (through **X-UserProfileID** entity header) in HTTP requests", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("user-profile", NULL, "set user profile filename. Content of file is appended as body to HTTP HEAD/GET requests, associated Mime is **text/xml**", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("query-string", NULL, "insert query string (without `?`) to URL on requests", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("dm-threads", NULL, "force using threads for async download requests rather than session scheduler", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("cte-rate-wnd", NULL, "set window analysis length in milliseconds for chunk-transfer encoding rate estimation", "20", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("cred", NULL, "path to 128 bits key for credential storage", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),

#ifdef GPAC_HAS_HTTP2
 GF_DEF_ARG("no-h2", NULL, "disable HTTP2", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("no-h2c", NULL, "disable HTTP2 upgrade (i.e. over non-TLS)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("h2-copy", NULL, "enable intermediate copy of data in nghttp2 (default is disabled but may report as broken frames in wireshark)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
#endif

 GF_DEF_ARG("dbg-edges", NULL, "log edges status in filter graph before dijkstra resolution (for debug). Edges are logged as edge_source(status, weight, src_cap_idx, dst_cap_idx)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("full-link", NULL, "throw error if any PID in the filter graph cannot be linked", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-dynf", NULL, "disable dynamically loaded filters", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),

 GF_DEF_ARG("no-block", NULL, "disable blocking mode of filters\n"
			"- no: enable blocking mode\n"
			"- fanout: disable blocking on fan-out, unblocking the PID as soon as one of its destinations requires a packet\n"
			"- all: disable blocking", "no", "no|fanout|all", GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-reg", NULL, "disable regulation (no sleep) in session", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-reassign", NULL, "disable source filter reassignment in PID graph resolution", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("sched", NULL, "set scheduler mode\n"
		"- free: lock-free queues except for task list (default)\n"
		"- lock: mutexes for queues when several threads\n"
		"- freex: lock-free queues including for task lists (experimental)\n"
		"- flock: mutexes for queues even when no thread (debug mode)\n"
		"- direct: no threads and direct dispatch of tasks whenever possible (debug mode)", "free", "free|lock|flock|freex|direct", GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("max-chain", NULL, "set maximum chain length when resolving filter links. Default value covers for __[ in -> ] dmx -> reframe -> decode -> encode -> reframe -> mx [ -> out]__. Filter chains loaded for adaptation (e.g. pixel format change) are loaded after the link resolution. Setting the value to 0 disables dynamic link resolution. You will have to specify the entire chain manually", "6", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("max-sleep", NULL, "set maximum sleep time slot in milliseconds when regulation is enabled", "50", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),

 GF_DEF_ARG("threads", NULL, "set N extra thread for the session. -1 means use all available cores", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-probe", NULL, "disable data probing on sources and relies on extension (faster load but more error-prone)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-argchk", NULL, "disable tracking of argument usage (all arguments will be considered as used)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("blacklist", NULL, "blacklist the filters listed in the given string (comma-separated list). If first character is '-', this is a whitelist, i.e. only filters listed in the given string will be allowed", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-graph-cache", NULL, "disable internal caching of filter graph connections. If disabled, the graph will be recomputed at each link resolution (lower memory usage but slower)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-reservoir", NULL, "disable memory recycling for packets and properties. This uses much less memory but stresses the system memory allocator much more", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("buffer-gen", NULL, "default buffer size in microseconds for generic pids", "1000", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("buffer-dec", NULL, "default buffer size in microseconds for decoder input pids", "1000000", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("buffer-units", NULL, "default buffer size in frames when timing is not available", "1", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),

 GF_DEF_ARG("gl-bits-comp", NULL, "number of bits per color component in OpenGL", "8", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-bits-depth", NULL, "number of bits for depth buffer in OpenGL", "16", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-doublebuf", NULL, "enable OpenGL double buffering", "yes", NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("glfbo-txid", NULL, "set output texture ID when using `glfbo` output. The OpenGL context shall be initialized and gf_term_process shall be called with the OpenGL context active", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("video-output", NULL, "indicate the name of the video output module to use (see `gpac -h modules`)."
	" The reserved name `glfbo` is used in player mode to draw in the OpenGL texture identified by [-glfbo-txid](). "
	" In this mode, the application is responsible for sending event to the compositor", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),

 GF_DEF_ARG("audio-output", NULL, "indicate the name of the audio output module to use", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_AUDIO),

 GF_DEF_ARG("font-reader", NULL, "indicate name of font reader module", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_TEXT),
 GF_DEF_ARG("font-dirs", NULL, "indicate comma-separated list of directories to scan for fonts", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_TEXT),
 GF_DEF_ARG("rescan-fonts", NULL, "indicate the font directory must be rescanned", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_TEXT),
 GF_DEF_ARG("wait-fonts", NULL, "wait for SVG fonts to be loaded before displaying frames", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_TEXT),
 GF_DEF_ARG("webvtt-hours", NULL, "force writing hour when serializing WebVTT", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_TEXT),
GF_DEF_ARG("charset", NULL, "set charset when not recognized from input. Possible values are:\n"
"- utf8: force UTF-8\n"
"- utf16: force UTF-16 little endian\n"
"- utf16be: force UTF-16 big endian\n"
"- other: attempt to parse anyway", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_TEXT),

 GF_DEF_ARG("rmt", NULL, "enable profiling through [Remotery](https://github.com/Celtoys/Remotery). A copy of Remotery visualizer is in gpac/share/vis, usually installed in __/usr/share/gpac/vis__ or __Program Files/GPAC/vis__", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-port", NULL, "set remotery port", "17815", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-reuse", NULL, "allow remotery to reuse port", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-localhost", NULL, "make remotery only accepts localhost connection", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-sleep", NULL, "set remotery sleep (ms) between server updates", "10", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-nmsg", NULL, "set remotery number of messages per update", "10", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-qsize", NULL, "set remotery message queue size in bytes", "131072", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-log", NULL, "redirect logs to remotery (experimental, usually not well handled by browser)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-ogl", NULL, "make remotery sample opengl calls", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),

 GF_DEF_ARG("m2ts-vvc-old", NULL, "hack for old TS streams using 0x32 for VVC instead of 0x33", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HACKS),
 GF_DEF_ARG("piff-force-subsamples", NULL, "hack for PIFF PSEC files generated by 0.9.0 and 1.0 MP4Box with wrong subsample_count inserted for audio", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HACKS),
 GF_DEF_ARG("vvdec-annexb", NULL, "hack for old vvdec+libavcodec supporting only annexB format", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HACKS),
 GF_DEF_ARG("heif-hevc-urn", NULL, "use HEVC URN for alpha and depth in HEIF instead of MPEG-B URN (HEIF first edition)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HACKS),
 GF_DEF_ARG("boxdir", NULL, "use box definitions in the given directory for XML dump", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HACKS),


 {0}
};


const GF_Config *gf_sys_get_lang_file();

GF_EXPORT
const GF_GPACArg *gf_sys_get_options()
{
	return GPAC_Args;
}

static const char *gpac_opt_default(const char *argname)
{
	const GF_GPACArg *arg = NULL;
	u32 i=0;
	while (GPAC_Args[i].name) {
		arg = &GPAC_Args[i];
		i++;
		if (!strcmp(arg->name, argname)) break;
		arg = NULL;
	}
	if (!arg) return NULL;
	return arg->val;
}

GF_EXPORT
Bool gf_opts_get_bool(const char *secName, const char *keyName)
{
	const char *opt = gf_opts_get_key(secName, keyName);

	if (!opt && !strcmp(secName, "core")) {
		opt = gpac_opt_default(keyName);
	}

	if (!opt) return GF_FALSE;
	if (!strcmp(opt, "yes")) return GF_TRUE;
	if (!strcmp(opt, "true")) return GF_TRUE;
	if (!strcmp(opt, "1")) return GF_TRUE;
	return GF_FALSE;
}
GF_EXPORT
u32 gf_opts_get_int(const char *secName, const char *keyName)
{
	u32 times=1, val;
	char *opt = (char *) gf_opts_get_key(secName, keyName);

	if (!opt && !strcmp(secName, "core")) {
		opt = (char *) gpac_opt_default(keyName);
	}
	if (!opt || !opt[0]) return 0;
	val = (u32) strlen(opt);
	char c = opt[val-1];
	switch (c) {
	case 'k':
	case 'K':
		times=1000;
		break;
	case 'm':
	case 'M':
		times=1000000;
		break;
	}
	val = atoi(opt);
	return val*times;
}

GF_EXPORT
Bool gf_sys_set_cfg_option(const char *opt_string)
{
	size_t sepIdx;
	char *sep, *sep2, szSec[1024], szKey[1024], szVal[1024];
	sep = strchr(opt_string, ':');
	if (!sep) {
		sep = strchr(opt_string, '=');
		if (sep && !stricmp(sep, "=null")) {
			sepIdx = sep - opt_string;
			if (sepIdx>=1024) sepIdx = 1023;
			strncpy(szSec, opt_string, sepIdx);
			szSec[sepIdx] = 0;
			gf_opts_del_section(szSec);
			return  GF_TRUE;
		}

		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreArgs] Badly formatted option %s - expected Section:Name=Value\n", opt_string ) );
		return GF_FALSE;
	}

	sepIdx = sep - opt_string;
	if (sepIdx>=1024)
		sepIdx = 1023;
	strncpy(szSec, opt_string, sepIdx);
	szSec[sepIdx] = 0;

	sep ++;
	sep2 = strchr(sep, '=');
	if (!sep2) {
		gf_opts_set_key(szSec, sep, NULL);
		return  GF_TRUE;
	}

	sepIdx = sep2 - sep;
	if (sepIdx>=1024)
		sepIdx = 1023;
	strncpy(szKey, sep, sepIdx);
	szKey[sepIdx] = 0;

	sepIdx = strlen(sep2+1);
	if (sepIdx>=1024)
		sepIdx = 1023;
	memcpy(szVal, sep2+1, sepIdx);
	szVal[sepIdx] = 0;

	if (!stricmp(szKey, "*")) {
		if (stricmp(szVal, "null")) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreArgs] Badly formatted option %s - expected Section:*=null\n", opt_string));
			return GF_FALSE;
		}
		gf_opts_del_section(szSec);
		return GF_TRUE;
	}

	if (!stricmp(szVal, "null")) {
		szVal[0]=0;
	}
	gf_opts_set_key(szSec, szKey, szVal[0] ? szVal : NULL);

	if (!strcmp(szSec, "core") || !strcmp(szSec, "temp")) {
		if (!strcmp(szKey, "noprog") && (!strcmp(szVal, "yes")||!strcmp(szVal, "true")||!strcmp(szVal, "1")) ) {
			void gpac_disable_progress();

			gpac_disable_progress();
		}
	}
	return GF_TRUE;
}

void gf_module_reload_dirs();

Bool gf_opts_load_option(const char *arg_name, const char *val, Bool *consumed_next, GF_Err *e)
{
	const GF_GPACArg *arg = NULL;
	u32 i=0;
	*e = GF_OK;
	*consumed_next = GF_FALSE;
	arg_name = arg_name+1;
	while (GPAC_Args[i].name) {
		arg = &GPAC_Args[i];
		i++;
		if (!strcmp(arg->name, arg_name)) break;
		if (arg->altname && !strcmp(arg->altname, arg_name)) break;

		arg = NULL;
	}
	if (!arg) return GF_FALSE;

	if (!strcmp(arg->name, "cfg")) {
		*consumed_next = GF_TRUE;
		if (val && strchr(val, '=')) {
			if (! gf_sys_set_cfg_option(val)) *e = GF_BAD_PARAM;
		} else {
			u32 sec_len = 0;
			char *sep = val ? strchr(val, ':') : NULL;
			u32 sec_count = gf_opts_get_section_count();
			if (sep) {
				sec_len = (u32) (sep - val - 1);
				sep++;
			} else if (val) {
				sec_len = (u32) strlen(val);
			}
			for (i=0; i<sec_count; i++) {
				u32 k, key_count;
				Bool sec_hdr_done = GF_FALSE;
				const char *sname = gf_opts_get_section_name(i);
				key_count = sname ? gf_opts_get_key_count(sname) : 0;
				if (!key_count) continue;

				if (sec_len) {
					if (!strncmp(val, "*", sec_len) || !strncmp(val, "@", sec_len)) {
					} else if (strncmp(val, sname, sec_len) || (sec_len != (u32) strlen(sname) ) ) {
						continue;
					}
				}
				for (k=0; k<key_count; k++) {
					const char *kname = gf_opts_get_key_name(sname, k);
					const char *kval = kname ? gf_opts_get_key(sname, kname) : NULL;
					if (!kval) continue;
					if (sep && strcmp(sep, kname)) continue;

					if (!sec_hdr_done) {
						sec_hdr_done = GF_TRUE;
						fprintf(stdout, "[%s]\n", sname);
					}
					fprintf(stdout, "%s=%s\n", kname, kval);
				}
				if (sec_hdr_done)
					fprintf(stdout, "\n");
			}
		}
		return GF_TRUE;
	}
	if (!strcmp(arg->name, "strict-error")) {
		gf_log_set_strict_error(1);
		return GF_TRUE;
	}

	if (arg->type==GF_ARG_BOOL) {
		if (!val) gf_opts_set_key("temp", arg->name, "yes");
		else {
			if (!strcmp(val, "yes") || !strcmp(val, "true") || !strcmp(val, "1")) {
				*consumed_next = GF_TRUE;
				gf_opts_set_key("temp", arg->name, "yes");
			} else {
				if (!strcmp(val, "no") || !strcmp(val, "false") || !strcmp(val, "0")) {
					*consumed_next = GF_TRUE;
					gf_opts_set_key("temp", arg->name, "no");
				} else {
					gf_opts_set_key("temp", arg->name, "yes");
				}
			}
		}
	} else {
		*consumed_next = GF_TRUE;
		if (!val && (arg->type==GF_ARG_BOOL))
			gf_opts_set_key("temp", arg->name, "true");
		else {
			gf_opts_set_key("temp", arg->name, val);
			if (!strcmp(arg->name, "mod-dirs")) {
				gf_module_reload_dirs();
			}
		}
	}
	return GF_TRUE;
}

GF_EXPORT
u32 gf_sys_is_gpac_arg(const char *arg_name)
{
	char *argsep;
	u32 arglen;
	const GF_GPACArg *arg = NULL;
	u32 i=0;
	arg_name = arg_name+1;
	if (arg_name[0]=='-')
		return 1;
	if (arg_name[0]=='+')
		return 1;

	argsep = strchr(arg_name, '=');
	if (argsep) arglen = (u32) (argsep - arg_name);
	else arglen = (u32) strlen(arg_name);

	while (GPAC_Args[i].name) {
		arg = &GPAC_Args[i];
		i++;
		if ((strlen(arg->name) == arglen) && !strncmp(arg->name, arg_name, arglen)) break;
		if (arg->altname) {
			char *alt = strstr(arg->altname, arg_name);
			if (alt) {
				char c = alt[strlen(arg_name)];
				if (!c || (c==' ')) break;
			}
		}
		arg = NULL;
	}
	if (!arg) return 0;
	if (arg->type==GF_ARG_BOOL) return 1;
	if (argsep) return 1;
	return 2;
}


GF_EXPORT
void gf_sys_print_arg(FILE *helpout, u32 flags, const GF_GPACArg *arg, const char *arg_subsystem)
{
	u32 gen_doc = 0;
	if (flags & GF_PRINTARG_MD)
		gen_doc = 1;
	if (!helpout) helpout = stderr;

//#ifdef GPAC_ENABLE_COVERAGE
#if 1
	if ((arg->name[0]>='A') && (arg->name[0]<='Z')) {
		if ((arg->name[1]<'A') || (arg->name[1]>'Z')) {
			fprintf(stderr, "\nWARNING: arg %s bad name format, should use lowercase\n", arg->name);
			exit(1);
		}
	}
	if (arg->description) {
		char *sep;

		if ((arg->description[0]>='A') && (arg->description[0]<='Z')) {
			if ((arg->description[1]<'A') || (arg->description[1]>'Z')) {
				fprintf(stderr, "\nWARNING: arg %s bad name format \"%s\", should use lowercase\n", arg->name, arg->description);
				exit(1);
			}
		}
		if (strchr(arg->description, '\t')) {
			fprintf(stderr, "\nWARNING: arg %s bad description format \"%s\", should not use tab\n", arg->name, arg->description);
			exit(1);
		}

		u8 achar = arg->description[strlen(arg->description)-1];
		if (achar == '.') {
			fprintf(stderr, "\nWARNING: arg %s bad description format \"%s\", should not end with .\n", arg->name, arg->description);
			exit(1);
		}
		sep = strstr(arg->description, ".\n");
		if (sep) {
			fprintf(stderr, "\nWARNING: arg %s bad description format \"%s\", should not contain .\\n \n", arg->name, arg->description);
			exit(1);
		}
		sep = strstr(arg->description, "- ");
		if (sep && (sep[-1]!='\n') && !strcmp(sep, "- see")) {
			fprintf(stderr, "\nWARNING: arg %s bad description format \"%s\", should have \\n before first bullet\n", arg->name, arg->description);
			exit(1);
		}

		sep = strchr(arg->description, ' ');
		if (sep && (sep>arg->description)) {
			sep--;
			if ((sep[0] == 's') && (sep[-1] != 's')) {
				fprintf(stderr, "\nWARNING: arg %s bad description format \"%s\", should use infintive\n", arg->name, arg->description);
				exit(1);
			}
		}
	}
#endif

	if (arg->flags & GF_ARG_HINT_HIDE)
		return;

	const char *syntax=strchr(arg->name, ' ');
	char *arg_name=NULL;
	if (syntax) {
		arg_name = gf_strdup(arg->name);
		char *sep = strchr(arg_name, ' ');
		sep[0]=0;
	}

	if (flags & GF_PRINTARG_MAN) {
		fprintf(helpout, ".TP\n.B %s%s", (flags&GF_PRINTARG_NO_DASH) ? "" : "\\-", arg_name ? arg_name : arg->name);
	}
	else if (gen_doc==1) {
		if (flags&GF_PRINTARG_NO_DASH) {
			gf_sys_format_help(helpout, flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s", arg_name ? arg_name : arg->name);
		} else {
			gf_sys_format_help(helpout, flags, "<a id=\"%s\">", arg_name ? arg_name : arg->name);
			gf_sys_format_help(helpout, flags | GF_PRINTARG_HIGHLIGHT_FIRST, "-%s", arg_name ? arg_name : arg->name);
			gf_sys_format_help(helpout, flags, "</a>");
		}
	} else {
		gf_sys_format_help(helpout, flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s%s%s",
			(flags&GF_PRINTARG_ADD_DASH) ? "-" : "",
			(flags&GF_PRINTARG_NO_DASH) ? "" : ((flags&GF_PRINTARG_COLON) ? ":" : "-"),
			arg_name ? arg_name : arg->name
		);
	}
	if (arg->altname) {
		gf_sys_format_help(helpout, flags, ",");
		gf_sys_format_help(helpout, flags | GF_PRINTARG_HIGHLIGHT_FIRST, "%s-%s", (flags&GF_PRINTARG_ADD_DASH) ? "-" : "", arg->altname);
	}
	if (syntax) {
		gf_sys_format_help(helpout, flags, " %s", syntax);
		gf_free(arg_name);
	}

	if (arg->type==GF_ARG_CUSTOM) {
		if (arg->val)
			gf_sys_format_help(helpout, flags, " `%s`", arg->val);
	} else if (arg->type==GF_ARG_INT && arg->values && strchr(arg->values, '|')) {
		gf_sys_format_help(helpout, flags, " (Enum");
		if (arg->val)
			gf_sys_format_help(helpout, flags, ", default: **%s**", arg->val);
		gf_sys_format_help(helpout, flags, ")");
	} else if (arg->type != GF_ARG_BOOL) {
		gf_sys_format_help(helpout, flags, " (");
		switch (arg->type) {
		case GF_ARG_BOOL: gf_sys_format_help(helpout, flags, "boolean"); break;
		case GF_ARG_INT: gf_sys_format_help(helpout, flags, "int"); break;
		case GF_ARG_DOUBLE: gf_sys_format_help(helpout, flags, "number"); break;
		case GF_ARG_STRING: gf_sys_format_help(helpout, flags, "string"); break;
		case GF_ARG_STRINGS: gf_sys_format_help(helpout, flags, "string list"); break;
		case GF_ARG_4CC: gf_sys_format_help(helpout, flags, "4CC"); break;
		case GF_ARG_4CCS: gf_sys_format_help(helpout, flags, "4CC list"); break;
		default: break;
		}
		if (arg->val)
			gf_sys_format_help(helpout, flags, ", default: **%s**", arg->val);
		if (arg->values)
			gf_sys_format_help(helpout, flags, ", values: __%s__", arg->values);
		gf_sys_format_help(helpout, flags, ")");
	}

	if (flags & GF_PRINTARG_MAN) {
		gf_sys_format_help(helpout, flags, "\n%s\n", gf_sys_localized(arg_subsystem, arg->name, arg->description) );
	} else {
		if (arg->description) {
			gf_sys_format_help(helpout, flags | GF_PRINTARG_OPT_DESC, ": %s", gf_sys_localized(arg_subsystem, arg->name, arg->description) );
		}
		gf_sys_format_help(helpout, flags, "\n");
	}

	if ((gen_doc==1) && arg->description && strstr(arg->description, "- "))
		gf_sys_format_help(helpout, flags, "\n");
}


GF_EXPORT
void gf_sys_print_core_help(FILE *helpout, u32 flags, GF_SysArgMode mode, u32 subsystem_flags)
{
	u32 i=0;
	const GF_GPACArg *args = gf_sys_get_options();

	while (args[i].name) {
		const GF_GPACArg *arg = &args[i];
		i++;
		if (arg->flags & GF_ARG_HINT_HIDE) continue;

		if (subsystem_flags && !(arg->flags & subsystem_flags)) {
		 	continue;
		}
		if (mode != GF_ARGMODE_ALL) {
			if ((mode==GF_ARGMODE_EXPERT) && !(arg->flags & GF_ARG_HINT_EXPERT)) continue;
			else if ((mode==GF_ARGMODE_ADVANCED) && !(arg->flags & GF_ARG_HINT_ADVANCED)) continue;
			else if ((mode==GF_ARGMODE_BASE) && (arg->flags & (GF_ARG_HINT_ADVANCED|GF_ARG_HINT_EXPERT) )) continue;
		}
		gf_sys_print_arg(helpout, flags, arg, "core");
	}
}


#define LINE_OFFSET_DESCR 30

static char *help_buf = NULL;
static u32 help_buf_size=0;

void gf_sys_cleanup_help()
{
	if (help_buf) {
		gf_free(help_buf);
		help_buf = NULL;
		help_buf_size = 0;
	}
}


enum
{
	TOK_CODE,
	TOK_BOLD,
	TOK_ITALIC,
	TOK_STRIKE,
	TOK_OPTLINK,
	TOK_LINKSTART,
};
struct _token {
	char *tok;
	GF_ConsoleCodes cmd_type;
} Tokens[] =
{
 {"`", GF_CONSOLE_YELLOW|GF_CONSOLE_ITALIC},
 {"**", GF_CONSOLE_BOLD},
 {"__", GF_CONSOLE_ITALIC},
 {"~~", GF_CONSOLE_STRIKE},
 {"[-", GF_CONSOLE_YELLOW|GF_CONSOLE_ITALIC},
 {"[", GF_CONSOLE_YELLOW|GF_CONSOLE_ITALIC},
};
static u32 nb_tokens = sizeof(Tokens) / sizeof(struct _token);

static u32 line_pos = 0;

//#define CHECK_BALANCED_SEPS

#ifdef CHECK_BALANCED_SEPS
static void check_char_balanced(char *buf, char c)
{
	char *txt = buf;
	while (txt) {
		char *bquote_next;
		char *bquote = strchr(txt, c);
		if (!bquote) break;
		if (c=='\'') {
			if ((bquote[1]=='s') && (bquote[2]==' ')) {
				txt = bquote + 1;
				continue;
			}
		}
		bquote_next = strchr(bquote+1, c);
		if (!bquote_next) {
			fprintf(stderr, "Missing closing %c after %s\n", c, bquote);
			exit(1);
		}
		switch (bquote_next[1] ) {
		case 0:
		case '\n':
		case ' ':
		case ',':
		case '.':
		case ')':
		case ']':
		case ':':
			break;
		default:
			if (c=='\'') {
				if ((bquote_next[1]>='A') && (bquote_next[1]<='Z'))
					break;
			}
			fprintf(stderr, "Missing space after closing %c %s\n", c, bquote_next);
			exit(1);
		}
		txt = bquote_next + 1;
	}
}
#endif

GF_EXPORT
void gf_sys_format_help(FILE *helpout, u32 flags, const char *fmt, ...)
{
	char *line;
	u32 len;
	va_list vlist;
	Bool escape_xml = GF_FALSE;
	Bool escape_pipe = GF_FALSE;
	Bool prev_was_example = GF_FALSE;
	Bool prev_has_line_after = GF_FALSE;
	u32 gen_doc = 0;
	u32 is_app_opts = 0;
	if (flags & GF_PRINTARG_MD) {
		gen_doc = 1;
		if (flags & GF_PRINTARG_ESCAPE_XML)
			escape_xml = GF_TRUE;
		if (flags & GF_PRINTARG_ESCAPE_PIPE)
			escape_pipe = GF_TRUE;
	}
	if (flags & GF_PRINTARG_MAN)
		gen_doc = 2;
	if (flags & GF_PRINTARG_IS_APP)
		is_app_opts = 1;
	if (!helpout) helpout = stderr;

	va_start(vlist, fmt);
	len=vsnprintf(NULL, 0, fmt, vlist);
	va_end(vlist);
	if (help_buf_size < len+2) {
		help_buf_size = len+2;
		help_buf = gf_realloc(help_buf, help_buf_size);
	}
	va_start(vlist, fmt);
	vsprintf(help_buf, fmt, vlist);
	va_end(vlist);

#ifdef CHECK_BALANCED_SEPS
	if (gen_doc) {
		check_char_balanced(help_buf, '`');
		check_char_balanced(help_buf, '\'');
	}
#endif

/*#ifdef GPAC_CONFIG_ANDROID
	//on android use logs for help print
	if (!gen_doc) {
		GF_LOG(GF_LOG_INFO, GF_LOG_APP, ("%s", help_buf));
		return;
	}
#endif
*/

	line = help_buf;
	while (line[0]) {
		u32 att_len = 0;
		char *tok_sep = NULL;
		GF_ConsoleCodes console_code = GF_CONSOLE_RESET;
		Bool line_before = GF_FALSE;
		Bool line_after = GF_FALSE;
		const char *footer_string = NULL;
		const char *header_string = NULL;
		char *next_line = strchr(line, '\n');
		Bool has_token=GF_FALSE;

		if (next_line) next_line[0]=0;

		if (prev_has_line_after && !strlen(line)) {
			if (!next_line) break;
			line = next_line+1;
			line_pos=0;
			continue;
		}

		if ((line[0]=='#') && (line[1]==' ')) {
			if (!gen_doc)
				line+=2;
			else if (gen_doc==2) {
				header_string = ".SH ";
				footer_string = "\n.LP";
				line+=2;
			}

			console_code = GF_CONSOLE_GREEN;
			line_after = line_before = GF_TRUE;
		} else if ((line[0]=='#') && (line[1]=='#') && (line[2]==' ')) {
			if (!gen_doc)
				line+=3;
			else if (gen_doc==2) {
				line+=3;
				header_string = ".SS ";
			}

			console_code = GF_CONSOLE_MAGENTA;
			line_before = GF_TRUE;
		} else if ((line[0]=='#') && (line[1]=='#') && (line[2]=='#') && (line[3]==' ')) {
			if (!gen_doc)
				line+=4;
			else if (gen_doc==2) {
				line+=4;
				header_string = ".P\n.B\n";
			}

			console_code = GF_CONSOLE_CYAN;
			line_after = GF_TRUE;
		} else if ((line[0]=='E') && (line[1]=='X') && (line[2]==' ')) {
			line+=3;
			console_code = GF_CONSOLE_YELLOW;

			if (gen_doc==1) {
				header_string = "Example\n```\n";
				footer_string = "\n```";
			} else if (gen_doc==2) {
				header_string = "Example\n.br\n";
				footer_string = "\n.br\n";
			} else {
				header_string = "Example:\n";
			}

			if (prev_was_example) {
				header_string = NULL;
			}

			if (next_line && (next_line[1]=='E') && (next_line[2]=='X') && (next_line[3]==' ')) {
				prev_was_example = GF_TRUE;
				footer_string = NULL;
			} else {
				prev_was_example = GF_FALSE;
			}
		} else if (!strncmp(line, "Note: ", 6)) {
			console_code = GF_CONSOLE_CYAN | GF_CONSOLE_ITALIC;
		} else if (!strncmp(line, "Warning: ", 9)) {
			line_after = line_before = GF_TRUE;
			console_code = GF_CONSOLE_RED | GF_CONSOLE_BOLD;
		} else if ( (
			 ((line[0]=='-') && (line[1]==' '))
			 || ((line[0]==' ') && (line[1]=='-') && (line[2]==' '))
			 || ((line[0]==' ') && (line[1]==' ') && (line[2]=='-') && (line[3]==' '))
			)

			//look for ": "
			&& ((tok_sep=strstr(line, ": ")) != NULL )
		) {
			if (!gen_doc)
				fprintf(helpout, "\t");
			while (line[0] != '-') {
				fprintf(helpout, " ");
				line++;
				line_pos++;

			}
			fprintf(helpout, "* ");
			line_pos+=2;
			if (!gen_doc)
				gf_sys_set_console_code(helpout, GF_CONSOLE_YELLOW);
			tok_sep[0] = 0;
			fprintf(helpout, "%s", line+2);
			line_pos += (u32) strlen(line+2);
			tok_sep[0] = ':';
			line = tok_sep;
			if (!gen_doc)
				gf_sys_set_console_code(helpout, GF_CONSOLE_RESET);
		} else if (flags & (GF_PRINTARG_HIGHLIGHT_FIRST | GF_PRINTARG_OPT_DESC)) {
			char *sep = strchr(line, ' ');

			if (sep) sep[0] = 0;

			if (!gen_doc && !(flags & GF_PRINTARG_OPT_DESC) )
				gf_sys_set_console_code(helpout, GF_CONSOLE_GREEN);

			if ((gen_doc==1) && !(flags & GF_PRINTARG_OPT_DESC) ) {
				fprintf(helpout, "__%s__", line);
				line_pos += 4+ (u32) strlen(line);
			} else {
				fprintf(helpout, "%s", line);
				line_pos += (u32) strlen(line);
			}

			if (!gen_doc && !(flags & GF_PRINTARG_OPT_DESC) )
				gf_sys_set_console_code(helpout, GF_CONSOLE_RESET);

			if (flags & GF_PRINTARG_OPT_DESC) {
				flags = 0;
				att_len = line_pos;
			}


			if (sep) {
				sep[0] = ' ';
				line = sep;
			} else {
				line = NULL;
			}
		}
		if (!line) break;
		if (gen_doc==2) {
			line_before = line_after = GF_FALSE;
		}

		if (prev_has_line_after) line_before = GF_FALSE;
		prev_has_line_after = GF_FALSE;
		if (!strlen(line))
			prev_has_line_after = GF_TRUE;

		if (line_before) {
			fprintf(helpout, "\n");
			line_pos=0;
		}

		if (console_code != GF_CONSOLE_RESET) {
			if (gen_doc==1) {
				if (console_code & GF_CONSOLE_BOLD) {
					fprintf(helpout, "__");
					line_pos+=2;
				}
				else if (console_code & GF_CONSOLE_ITALIC) {
					fprintf(helpout, "_");
					line_pos++;
				}
			} else if (!gen_doc) {
				gf_sys_set_console_code(helpout, console_code);
			}
		}

		if (att_len) {
			while (att_len < LINE_OFFSET_DESCR) {
				fprintf(helpout, " ");
				att_len++;
				line_pos++;
			}
		}


		if (header_string) {
			fprintf(helpout, "%s", header_string);
			line_pos += (u32) strlen(header_string);
		}

		while (line) {
			char *skip_url = NULL;
			char *link_start = NULL;
			u32 tid=0, i;
			char *next_token = NULL;
			for (i=0; i<nb_tokens; i++) {
				char *tok = strstr(line, Tokens[i].tok);
				if (!tok) continue;
				if (next_token && ((next_token-line) < (tok-line)) ) continue;
				//check we have an end of token, otherwise consider this regular text
				if ((i == TOK_LINKSTART) || (i == TOK_OPTLINK)) {
					char *end_tok = strstr(tok, "](");
					if (!end_tok) continue;
				}

				if (i == TOK_LINKSTART) {
					if (tid == TOK_OPTLINK) continue;
					if (gen_doc!=1) {
						char *link_end;
						skip_url = strstr(tok, "](");
						link_end = skip_url;
						if (skip_url) skip_url = strstr(skip_url, ")");
						if (skip_url) skip_url ++;

						if (!skip_url) continue;
						link_start = tok+1;
						link_end[0] = 0;
					} else {
						continue;
					}
				}
				next_token=tok;
				tid=i;
			}
			if (next_token) {
				next_token[0]=0;
			}
			if ((gen_doc==1) && has_token) {
				if (tid==TOK_CODE) {
					fprintf(helpout, "`%s`", line);
					line_pos+=2;
				} else if (tid==TOK_ITALIC) {
					fprintf(helpout, "_%s_", line);
					line_pos+=2;
				} else if (tid==TOK_BOLD) {
					fprintf(helpout, "__%s__", line);
					line_pos+=4;
				} else {
					fprintf(helpout, "%s", line);
				}
			} else if (escape_xml) {
				char *xml_line = line;
				while (xml_line) {
					char *xml_start = strchr(xml_line, '<');
					char *xml_end = strchr(xml_line, '>');

					if (xml_end && (xml_start > xml_end)) xml_start = xml_end;
					else if (!xml_start && xml_end) xml_start = xml_end;
					else if (xml_start && xml_end) xml_end = NULL;

					if (xml_start) {
						u8 c = xml_start[0];
						xml_start[0] = 0;
						fprintf(helpout, "%s", xml_line);
						fprintf(helpout, xml_end ? "&gt;" : "&lt;");
						xml_start[0] = c;
						xml_line = xml_start+1;
					} else {
						fprintf(helpout, "%s", xml_line);
						break;
					}
				}
			} else if (escape_pipe) {
				char *src_line = line;
				while (src_line) {
					char *pipe_start = strchr(src_line, '|');
					if (pipe_start && (pipe_start[1]==' '))
						pipe_start = NULL;

					if (pipe_start) {
						pipe_start[0] = 0;
						fprintf(helpout, "%s ", src_line);
						pipe_start[0] = '|';
						src_line = pipe_start+1;
					} else {
						fprintf(helpout, "%s", src_line);
						break;
					}
				}
			} else {
				fprintf(helpout, "%s", line);
			}
			line_pos+=(u32) strlen(line);

			if (!next_token) break;
			has_token = !has_token;

			if (!gen_doc) {
				if (has_token) {
					u32 cmd;
					if (Tokens[tid].cmd_type & 0xFFFF) {
						cmd = Tokens[tid].cmd_type;
					} else {
						cmd = Tokens[tid].cmd_type | console_code;
					}

					if (console_code&GF_CONSOLE_ITALIC) {
						if (Tokens[tid].cmd_type & GF_CONSOLE_ITALIC) {
							cmd &= ~GF_CONSOLE_ITALIC;
							cmd |= GF_CONSOLE_BOLD;
						}
					}
					else if (console_code&GF_CONSOLE_BOLD) {
						if (Tokens[tid].cmd_type & GF_CONSOLE_BOLD) {
							cmd &= ~GF_CONSOLE_BOLD;
							cmd |= GF_CONSOLE_ITALIC;
						}
					}
					gf_sys_set_console_code(helpout, cmd);
				} else {
					gf_sys_set_console_code(helpout, console_code);
				}
			}
			line = next_token + (u32) strlen(Tokens[tid].tok);

			if (skip_url) {
				if (link_start)
					fprintf(helpout, "%s", link_start);
				if (!gen_doc)
					gf_sys_set_console_code(helpout, GF_CONSOLE_RESET);
				has_token = GF_FALSE;
				line = skip_url;

			}

			if (has_token && tid==TOK_OPTLINK) {
				char *link = strchr(line, '(');
				assert(link);
				link++;
				char *end_link = strchr(line, ')');
				if (end_link) end_link[0] = 0;
				char *end_tok = strchr(line, ']');
				if (end_tok) end_tok[0] = 0;

				if (gen_doc==1) {
					if (!strncmp(link, "GPAC", 4)) {
						fprintf(helpout, "[-%s](gpac_general/#%s)", line, line);
						line_pos+=7 + 2*(u32)strlen(line) + (u32)strlen("gpac_general");
					} else if (!strncmp(link, "LOG", 3)) {
						fprintf(helpout, "[-%s](core_logs/#%s)", line, line);
						line_pos+=7 + 2* (u32)strlen(line) + (u32)strlen("core_logs");
					} else if (!strncmp(link, "CORE", 4)) {
						fprintf(helpout, "[-%s](core_options/#%s)", line, line);
						line_pos+=7 + 2* (u32)strlen(line) + (u32)strlen("core_options");
					} else if (!strncmp(link, "CFG", 3)) {
						fprintf(helpout, "[-%s](core_config/#%s)", line, line);
						line_pos+=7 + 2*(u32)strlen(line) + (u32)strlen("core_config");
					} else if (!strncmp(link, "MP4B_GEN", 8)) {
						fprintf(helpout, "[-%s](mp4box-gen-opts/#%s)", line, line);
						line_pos+=7 + 2* (u32)strlen(line) + (u32)strlen("mp4box-gen-opts");
					} else if (strlen(link)) {
						fprintf(helpout, "[-%s](%s/#%s)", line, link, line);
						line_pos+=7 + 2* (u32)strlen(line) + (u32)strlen(link);
					} else if (is_app_opts || !strcmp(line, "i") || !strcmp(line, "o") || !strcmp(line, "h")) {
						fprintf(helpout, "[-%s](#%s)", line, line);
						line_pos+=6 + 2* (u32)strlen(line);
					} else {
						//this is a filter opt, don't print '-'
						fprintf(helpout, "[%s](#%s)", line, line);
						line_pos+=5 + 2* (u32)strlen(line);
					}
				} else {
					if (gen_doc==2)
						fprintf(helpout, ".I ");

					if (!strncmp(link, "GPAC", 4)
						|| !strncmp(link, "LOG", 3)
						|| !strncmp(link, "CORE", 4)
						|| strlen(link)
						|| !strcmp(line, "i") || !strcmp(line, "o") || !strcmp(line, "h")
					) {
						fprintf(helpout, "-%s", line);
						line_pos+=1+ (u32)strlen(line);
					} else {
						fprintf(helpout, "%s", line);
						line_pos+= (u32)strlen(line);
					}
					if (!gen_doc)
						gf_sys_set_console_code(helpout, GF_CONSOLE_RESET);
				}
				if (!end_link) break;
				line = end_link+1;
				has_token = GF_FALSE;
			}
		}

		if (has_token && !gen_doc)
			gf_sys_set_console_code(helpout, GF_CONSOLE_RESET);

		if (footer_string) {
			fprintf(helpout, "%s", footer_string);
			line_pos += (u32) strlen(footer_string);
		}
		if (console_code != GF_CONSOLE_RESET) {
			if (gen_doc==1) {
				if (console_code & GF_CONSOLE_BOLD) {
					fprintf(helpout, "__");
					line_pos+=2;
				} else if (console_code & GF_CONSOLE_ITALIC) {
					fprintf(helpout, "_");
					line_pos++;
				}
			} else if (!gen_doc) {
				gf_sys_set_console_code(helpout, GF_CONSOLE_RESET);
			}
		}

		if (line_after) {
			if (gen_doc==1) fprintf(helpout, "  ");
			fprintf(helpout, (flags & GF_PRINTARG_NL_TO_BR) ? "<br/>" : "\n");
			line_pos=0;
			prev_has_line_after = GF_TRUE;
		}

		if (!next_line) break;
		next_line[0]=0;
		if (gen_doc==1) fprintf(helpout, "  ");
		line = next_line+1;
		if (gen_doc==2) {
			if (line[0] != '.')
				fprintf(helpout, "\n.br\n");
			else
				fprintf(helpout, "\n");
		} else
			fprintf(helpout, (line[0] && (flags & GF_PRINTARG_NL_TO_BR)) ? "<br/>" : "\n");
		line_pos=0;
	}
}

GF_EXPORT
Bool gf_strnistr(const char *text, const char *subtext, u32 subtext_len)
{
	if (!*text || !subtext || !subtext_len)
		return GF_FALSE;

	while (*text) {
		if (tolower(*text) == *subtext) {
			if (!strnicmp(text, subtext, subtext_len))
				return GF_TRUE;

		}
		text++;
	}
	return GF_FALSE;
}

GF_EXPORT
Bool gf_sys_word_match(const char *orig, const char *dst)
{
	s32 dist = 0;
	u32 match = 0;
	u32 i;
	u32 olen = (u32) strlen(orig);
	u32 dlen = (u32) strlen(dst);
	u32 *run;

	if ((olen>=3) && (olen<dlen) && !strncmp(orig, dst, olen))
		return GF_TRUE;
	if ((dlen>=3) && (dlen<olen) && !strncmp(orig, dst, dlen))
		return GF_TRUE;

	if (olen*2 < dlen) {
		char *s1 = strchr(orig, ':');
		char *s2 = strchr(dst, ':');
		if (s1 && !s2) return GF_FALSE;
		if (!s1 && s2) return GF_FALSE;

		if (gf_strnistr(dst, orig, MIN(olen, dlen)))
			return GF_TRUE;
		return GF_FALSE;
	}

	if ((dlen>=3) && gf_strnistr(orig, dst, dlen))
		return GF_TRUE;
	if ((olen>=3) && gf_strnistr(dst, orig, olen))
		return GF_TRUE;

	run = gf_malloc(sizeof(u32) * olen);
	memset(run, 0, sizeof(u32) * olen);

	for (i=0; i<dlen; i++) {
		u32 dist_char;
		u32 offset=0;
		char *pos;

retry_char:
		pos = strchr(orig+offset, dst[i]);
		if (!pos) continue;
		dist_char = (u32) (pos - orig);
		if (!run[dist_char]) {
			run[dist_char] = i+1;
			match++;
		} else if (run[dist_char] > i) {
			run[dist_char] = i+1;
			match++;
		} else {
			//this can be a repeated character
			offset++;
			goto retry_char;

		}
	}
	if (match*2<olen) {
		gf_free(run);
		return GF_FALSE;
	}
/*
	//if 4/5 of characters are matched, suggest it
	if (match * 5 >= 4 * dlen ) {
		gf_free(run);
		return GF_TRUE;
	}
	if ((olen<=4) && (match>=3) && (dlen*2<olen*3) ) {
		gf_free(run);
		return GF_TRUE;
	}
*/
	for (i=0; i<olen; i++) {
		if (!i) {
			if (run[0]==1)
				dist++;
		} else if (run[i-1] + 1 == run[i]) {
			dist++;
		}
	}
	gf_free(run);
	//if half the characters are in order, consider a match
	//if arg is small only check dst
	if ((olen<=4) && (dist >= 2))
		return GF_TRUE;
	if ((dist*2 >= (s32) olen) && (dist*2 >= (s32) dlen))
		return GF_TRUE;
	return GF_FALSE;
}
