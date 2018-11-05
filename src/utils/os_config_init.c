/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2000-2018
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

#ifndef GPAC_DISABLE_CORE_TOOLS

#include <gpac/config_file.h>


#if defined(WIN32) || defined(_WIN32_WCE)
#include <windows.h> /*for GetModuleFileName*/

#ifndef _WIN32_WCE
#include <direct.h>  /*for _mkdir*/
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
#define TEST_MODULE     "osmo4ios"
#else
#define TEST_MODULE		"gm_"
#endif
#define CFG_FILE_NAME	"GPAC.cfg"

#else
#ifdef GPAC_CONFIG_LINUX
#include <unistd.h>
#endif
#ifdef GPAC_CONFIG_ANDROID
#define DEFAULT_ANDROID_PATH_APP	"/data/data/com.gpac.Osmo4"
#define DEFAULT_ANDROID_PATH_CFG	"/sdcard/osmo"
#endif
#define CFG_FILE_NAME	"GPAC.cfg"

#if defined(GPAC_CONFIG_WIN32)
#define TEST_MODULE		"gm_"
#else
#define TEST_MODULE		"gm_"
#endif

#endif

static Bool mod_enum(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	if (!strncmp(item_name, "gm_", 3) || !strncmp(item_name, "gf_", 3)) {
		*(Bool*)cbck = GF_TRUE;
		return GF_TRUE;
	}
	return GF_FALSE;
}

static Bool check_file_exists(char *name, char *path, char *outPath)
{
	char szPath[GF_MAX_PATH];
	FILE *f;

	if (! gf_dir_exists(path)) return 0;

	if (!strcmp(name, TEST_MODULE)) {
		Bool res = GF_FALSE;
		gf_enum_directory(path, GF_FALSE, mod_enum, &res, NULL);
		if (!res) return GF_FALSE;
		if (outPath != path) strcpy(outPath, path);
		return 1;
	}

	sprintf(szPath, "%s%c%s", path, GF_PATH_SEPARATOR, name);
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
};

#if defined(WIN32) || defined(_WIN32_WCE)
static Bool get_default_install_path(char *file_path, u32 path_type)
{
	FILE *f;
	char *sep;
	char szPath[GF_MAX_PATH];


#ifdef _WIN32_WCE
	TCHAR w_szPath[GF_MAX_PATH];
	GetModuleFileName(NULL, w_szPath, GF_MAX_PATH);
	CE_WideToChar((u16 *) w_szPath, file_path);
#else
	GetModuleFileNameA(NULL, file_path, GF_MAX_PATH);
#endif

	/*remove exe name*/
	if (strstr(file_path, ".exe")) {
		sep = strrchr(file_path, '\\');
		if (sep) sep[0] = 0;
	}

	strcpy(szPath, file_path);
	strlwr(szPath);

	/*if this is run from a browser, we do not get our app path - fortunately on Windows, we always use 'GPAC' in the
	installation path*/
	if (!strstr(file_path, "gpac") && !strstr(file_path, "GPAC") ) {
		HKEY hKey = NULL;
		DWORD dwSize = GF_MAX_PATH;

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

	/*we are looking for the config file path - make sure it is writable*/
	assert(path_type == GF_PATH_CFG);

	strcpy(szPath, file_path);
	strcat(szPath, "\\gpaccfgtest.txt");
	//do not use gf_fopen here, we don't want to through any error if failure
	f = fopen(szPath, "wb");
	if (f != NULL) {
		fclose(f);
		gf_delete_file(szPath);
		return GF_TRUE;
	}
#ifdef _WIN32_WCE
	return 0;
#else
	/*no write access, get user home directory*/
	SHGetFolderPath(NULL, CSIDL_APPDATA | CSIDL_FLAG_CREATE, NULL, SHGFP_TYPE_CURRENT, file_path);
	if (file_path[strlen(file_path)-1] != '\\') strcat(file_path, "\\");
	strcat(file_path, "GPAC");
	/*create GPAC dir*/
	_mkdir(file_path);
	strcpy(szPath, file_path);
	strcat(szPath, "\\gpaccfgtest.txt");
	f = fopen(szPath, "wb");
	/*COMPLETE FAILURE*/
	if (!f) return GF_FALSE;

	fclose(f);
	gf_delete_file(szPath);
	return GF_TRUE;
#endif
}

/*FIXME - the paths defined here MUST be coherent with the paths defined in applications/osmo4_android/src/com/gpac/Osmo4/GpacConfig.java'*/
#elif defined(GPAC_CONFIG_ANDROID)

static Bool get_default_install_path(char *file_path, u32 path_type)
{
	if (path_type==GF_PATH_APP) {
		strcpy(file_path, DEFAULT_ANDROID_PATH_APP);
		return 1;
	} else if (path_type==GF_PATH_CFG) {
		strcpy(file_path, DEFAULT_ANDROID_PATH_CFG);
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
#define SYMBIAN_GPAC_CFG_DIR	"\\system\\apps\\Osmo4"
#define SYMBIAN_GPAC_GUI_DIR	"\\system\\apps\\Osmo4\\gui"
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

static Bool get_default_install_path(char *file_path, u32 path_type)
{
	char app_path[GF_MAX_PATH];
	char *sep;
	u32 size = GF_MAX_PATH;

	/*on OSX, Linux & co, user home is where we store the cfg file*/
	if (path_type==GF_PATH_CFG) {
		char *user_home = getenv("HOME");
#ifdef GPAC_CONFIG_IOS
		char buf[PATH_MAX];
		char *res;
#endif
		if (!user_home) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Couldn't find HOME directory\n"));
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
			gf_delete_file(app_path);
		}

		strcat(file_path, "/.gpac");
		if (!gf_dir_exists(file_path)) {
			gf_mkdir(file_path);
		}
		return 1;
	}

	if (path_type==GF_PATH_APP) {
#if (defined(__DARWIN__) || defined(__APPLE__) )
		if (_NSGetExecutablePath(app_path, &size) ==0) {
			realpath(app_path, file_path);
			char *sep = strrchr(file_path, '/');
			if (sep) sep[0] = 0;
			return 1;
		}

#elif defined(GPAC_CONFIG_LINUX)
		size = readlink("/proc/self/exe", file_path, GF_MAX_PATH-1);
		if (size>0) {
			char *sep;
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
#endif
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Unknown arch, cannot find executable path\n"));
		return 0;
	}


	/*locate the app*/
	if (!get_default_install_path(app_path, GF_PATH_APP)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Couldn't find GPAC binaries install directory\n"));
		return 0;
	}

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
		if (get_default_install_path(app_path, GF_PATH_CFG)) {
			/*GUI not found, look in ~/.gpac/share/gui/ */
			strcat(app_path, "/.gpac/share");
			if (check_file_exists("share/gui.bt", app_path, file_path)) return 1;
		}

		/*GUI not found, look in gpac distribution if any */
		if (get_default_install_path(app_path, GF_PATH_APP)) {
			char *sep = strstr(app_path, "/bin/");
			if (sep) {
				sep[0] = 0;
				strcat(app_path, "/share");
				if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
			}
			sep = strstr(app_path, "/build/");
			if (sep) {
				sep[0] = 0;
				strcat(app_path, "/share");
				if (check_file_exists("gui/gui.bt", app_path, file_path)) return 1;
			}
		}
		/*GUI not found, look in .app for OSX case*/
	}

	if (path_type==GF_PATH_MODULES) {
		/*look in gpac compilation tree (modules are output in the same folder as apps) */
		if (get_default_install_path(app_path, GF_PATH_APP)) {
			if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;
			/*on OSX check modules subdirectory */
			strcat(app_path, "/modules");
			if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;
			/*modules not found*/
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Couldn't find any modules in standard path (app path %s)\n", app_path));
		}
		/*modules not found, look in ~/.gpac/modules/ */
		if (get_default_install_path(app_path, GF_PATH_CFG)) {
			strcpy(app_path, file_path);
			strcat(app_path, "/.gpac/modules");
			if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;
		}
		/*modules not found, failure*/
		GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Couldn't find any modules in HOME path (app path %s)\n", app_path));
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
static void gf_ios_refresh_cache_directory( GF_Config *cfg, char *file_path)
{
	char *cache_dir, *old_cache_dir;
	char buf[GF_MAX_PATH], *res, *sep;
	res = realpath(file_path, buf);
	if (!res) return;

	sep = strstr(res, ".gpac");
	assert(sep);
	sep[0] = 0;
	gf_cfg_set_key(cfg, "General", "LastWorkingDir", res);
	gf_cfg_set_key(cfg, "General", "iOSDocumentsDir", res);

	strcat(res, "cache/");
	cache_dir = res;
	old_cache_dir = (char*) gf_opts_get_key("core", "cache");

	if (!gf_dir_exists(cache_dir)) {
		if (old_cache_dir && strcmp(old_cache_dir, cache_dir)) {
			GF_LOG(GF_LOG_WARNING, GF_LOG_CORE, ("Cache dir changed: old %d -> new %s\n\n", old_cache_dir, cache_dir ));
		}
		gf_mkdir(cache_dir);
	}
	gf_opts_set_key("core", "cache", cache_dir);
}

#endif


static GF_Config *create_default_config(char *file_path, const char *profile)
{
	FILE *f;
	GF_Config *cfg;
	char szProfilePath[GF_MAX_PATH];
	char szPath[GF_MAX_PATH];
	char gui_path[GF_MAX_PATH];

	if (! get_default_install_path(file_path, GF_PATH_CFG)) {
		return NULL;
	}
	/*Create the config file*/
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
	if (!cfg) return NULL;
	strcpy(szProfilePath, szPath);


#ifndef GPAC_CONFIG_IOS
	if (! get_default_install_path(szPath, GF_PATH_MODULES)) {
		gf_delete_file(szPath);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] default modules not found\n"));
		return NULL;
	}
#else
	get_default_install_path(szPath, GF_PATH_APP);
#endif

#if defined(GPAC_CONFIG_IOS)
	gf_cfg_set_key(cfg, "General", "DeviceType", "iOS");
#elif defined(GPAC_CONFIG_ANDROID)
	gf_cfg_set_key(cfg, "General", "DeviceType", "Android");
#else
	gf_cfg_set_key(cfg, "General", "DeviceType", "Desktop");
#endif



	gf_cfg_set_key(cfg, "core", "mod-dirs", szPath);

#if defined(GPAC_CONFIG_IOS)
	gf_ios_refresh_cache_directory(cfg, file_path);
#elif defined(GPAC_CONFIG_ANDROID)
	if (get_default_install_path(szPath, GF_PATH_APP)) {
		strcat(szPath, "/cache");
		gf_cfg_set_key(cfg, "core", "cache", szPath);
	}
#else
	/*get default temporary directoy */
	gf_cfg_set_key(cfg, "core", "cache", gf_get_default_cache_directory());
#endif

	gf_cfg_set_key(cfg, "core", "raster2d", "GPAC 2D Raster");
	gf_cfg_set_key(cfg, "core", "ds-disable-notif", "no");

	/*Setup font engine to FreeType by default, and locate TrueType font directory on the system*/
	gf_cfg_set_key(cfg, "core", "font-reader", "FreeType Font Reader");
	gf_cfg_set_key(cfg, "core", "rescan-fonts", "yes");


#if defined(_WIN32_WCE)
	/*FIXME - is this true on all WinCE systems??*/
	strcpy(szPath, "\\Windows");
#elif defined(WIN32)
	GetWindowsDirectory((char*)szPath, MAX_PATH);
	if (szPath[strlen((char*)szPath)-1] != '\\') strcat((char*)szPath, "\\");
	strcat((char *)szPath, "Fonts");
#elif defined(__APPLE__)

#ifdef GPAC_CONFIG_IOS
	strcpy(szPath, "/System/Library/Fonts/Cache,/System/Library/Fonts/AppFonts,/System/Library/Fonts/Core,/System/Library/Fonts/Extra");
#else
	strcpy(szPath, "/Library/Fonts");
#endif

#elif defined(GPAC_CONFIG_ANDROID)
	strcpy(szPath, "/system/fonts/");
#else
	strcpy(szPath, "/usr/share/fonts/truetype/");
#endif
	gf_cfg_set_key(cfg, "core", "font-dirs", szPath);

	gf_cfg_set_key(cfg, "core", "cache-size", "100M");

#if defined(_WIN32_WCE)
	gf_cfg_set_key(cfg, "core", "video-output", "GAPI Video Output");
#elif defined(WIN32)
	gf_cfg_set_key(cfg, "core", "video-output", "DirectX Video Output");
#elif defined(__DARWIN__) || defined(__APPLE__)
	gf_cfg_set_key(cfg, "core", "video-output", "SDL Video Output");
#elif defined(GPAC_CONFIG_ANDROID)
	gf_cfg_set_key(cfg, "core", "video-output", "Android Video Output");
	gf_cfg_set_key(cfg, "core", "audio-output", "Android Audio Output");
#else
	gf_cfg_set_key(cfg, "core", "video-output", "X11 Video Output");
	gf_cfg_set_key(cfg, "core", "audio-output", "SDL Audio Output");
#endif

	gf_cfg_set_key(cfg, "core", "switch-vres", "no");
	gf_cfg_set_key(cfg, "core", "hwvmem", "auto");


	/*locate GUI*/
	if ( get_default_install_path(szPath, GF_PATH_SHARE) ) {
		sprintf(gui_path, "%s%cgui%cgui.bt", szPath, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		if (gf_file_exists(gui_path)) {
			gf_cfg_set_key(cfg, "General", "StartupFile", gui_path);
		}

		/*shaders are at the same location*/
		sprintf(gui_path, "%s%cshaders%cvertex.glsl", szPath, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		gf_cfg_set_key(cfg, "core", "vert-shader", gui_path);
		sprintf(gui_path, "%s%cshaders%cfragment.glsl", szPath, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		gf_cfg_set_key(cfg, "core", "frag-shader", gui_path);
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
	char *cfg_path;
	if ( get_default_install_path(path, GF_PATH_SHARE) ) {
		char *sep;
		char shader_path[GF_MAX_PATH];
		strcat(path, "/gui/gui.bt");
		gf_cfg_set_key(cfg, "General", "StartupFile", path);
		//get rid of "/gui/gui.bt"
		sep = strrchr(path, '/');
		sep[0] = 0;
		sep = strrchr(path, '/');
		sep[0] = 0;

		sprintf(shader_path, "%s%cshaders%cvertex.glsl", path, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		gf_cfg_set_key(cfg, "core", "vert-shader", shader_path);
		sprintf(shader_path, "%s%cshaders%cfragment.glsl", path, GF_PATH_SEPARATOR, GF_PATH_SEPARATOR);
		gf_cfg_set_key(cfg, "core", "frag-shader", shader_path);
	}
	cfg_path = gf_cfg_get_filename(cfg);
	gf_ios_refresh_cache_directory(cfg, cfg_path);
	gf_free(cfg_path);

#else
	const char *opt;

	if ( get_default_install_path(path, GF_PATH_MODULES) ) {
		opt = gf_cfg_get_key(cfg, "core", "mod-dirs");
		//for OSX, we can have an install in /usr/... and an install in /Applications/Osmo4.app - always change
#if defined(__DARWIN__) || defined(__APPLE__)
		if (!opt || strcmp(opt, path))
			gf_cfg_set_key(cfg, "core", "mod-dirs", path);
#else

		//otherwise only check we didn't switch between a 64 bit version and a 32 bit version
		if (!opt) {
			gf_cfg_set_key(cfg, "core", "mod-dirs", path);
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
				gf_cfg_set_key(cfg, "core", "mod-dirs", path);
			}
		}
#endif
	}

	/*if startup file was disabled, do not attempt to correct it*/
	if (gf_cfg_get_key(cfg, "General", "StartupFile")==NULL) return;

	if ( get_default_install_path(path, GF_PATH_SHARE) ) {
		opt = gf_cfg_get_key(cfg, "General", "StartupFile");
		if (strstr(opt, "gui.bt") && strcmp(opt, path) && strstr(path, ".app") ) {
#if defined(__DARWIN__) || defined(__APPLE__)
			strcat(path, "/gui/gui.bt");
			gf_cfg_set_key(cfg, "General", "StartupFile", path);
#endif
		}
	}

#endif
}


/*!
 *	\brief configuration file initialization
 *
 * Constructs a configuration file from fileName. If fileName is NULL, the default GPAC configuration file is loaded with the
 * proper module directory, font directory and other default options. If fileName is non-NULL no configuration file is found,
 * a "light" default configuration file is created.
 *\param profile name or path to existing config file
 *\return the configuration file object, NULL if the file could not be created
 */
static GF_Config *gf_cfg_init(const char *profile)
{
	GF_Config *cfg;
	char szPath[GF_MAX_PATH];

	if (profile && (strchr(profile, '/') || strchr(profile, '\\')) ) {
		if (!gf_file_exists(profile)) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Config file %s does not exist\n", profile));
			return NULL;
		}
		cfg = gf_cfg_new(NULL, profile);
		if (!cfg) {
			GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Failed to load existing config file %s\n", profile));
			return NULL;
		}
		check_modules_dir(cfg);
		return cfg;
	}

	if (!get_default_install_path(szPath, GF_PATH_CFG)) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("Fatal error: Cannot create global config  file in application or user home directory - no write access\n"));
		return NULL;
	}

	if (profile) {
		strcat(szPath, "/profiles/");
		strcat(szPath, profile);
	}

	cfg = gf_cfg_new(szPath, CFG_FILE_NAME);
	if (!cfg) {
		GF_LOG(GF_LOG_INFO, GF_LOG_CORE, ("GPAC config file %s not found in %s - creating new file\n", CFG_FILE_NAME, szPath ));
		cfg = create_default_config(szPath, profile);
	}
	if (!cfg) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("Cannot create config file %s in %s directory\n", CFG_FILE_NAME, szPath));
		return NULL;
	}

#ifndef GPAC_CONFIG_IOS
	GF_LOG(GF_LOG_DEBUG, GF_LOG_CORE, ("Using global config file in %s directory\n", szPath));
#endif

	check_modules_dir(cfg);

	if (!gf_cfg_get_key(cfg, "core", "store-dir")) {
		char *sep;
		strcpy(szPath, gf_cfg_get_filename(cfg));
		sep = strrchr(szPath, '/');
		if (!sep) sep = strrchr(szPath, '\\');
		if (sep) sep[0] = 0;
		strcat(szPath, "/Storage");
		if (!gf_dir_exists(szPath)) gf_mkdir(szPath);
		gf_cfg_set_key(cfg, "core", "store-dir", szPath);
	}
	return cfg;
}

GF_EXPORT
Bool gf_get_default_shared_directory(char *path_buffer)
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

		gf_modules_new(gpac_global_config);
	}
}

void gf_uninit_global_config()
{
	if (gpac_global_config) {
		gf_cfg_del(gpac_global_config);
		gpac_global_config = NULL;
		gf_modules_del();
	}
}

GF_EXPORT
const char *gf_opts_get_key(const char *secName, const char *keyName)
{
	if (!strcmp(secName, "core")) {
		const char *opt = gf_cfg_get_key(gpac_global_config, "temp", keyName);
		if (opt) return opt;
	}
	return gf_cfg_get_key(gpac_global_config, secName, keyName);
}
GF_EXPORT
GF_Err gf_opts_set_key(const char *secName, const char *keyName, const char *keyValue)
{
	return gf_cfg_set_key(gpac_global_config, secName, keyName, keyValue);
}
GF_EXPORT
void gf_opts_del_section(const char *secName)
{
	gf_cfg_del_section(gpac_global_config, secName);
}
GF_EXPORT
u32 gf_opts_get_section_count()
{
	return gf_cfg_get_section_count(gpac_global_config);
}
GF_EXPORT
const char *gf_opts_get_section_name(u32 secIndex)
{
	return gf_cfg_get_section_name(gpac_global_config, secIndex);
}
GF_EXPORT
u32 gf_opts_get_key_count(const char *secName)
{
	return gf_cfg_get_key_count(gpac_global_config, secName);
}
GF_EXPORT
const char *gf_opts_get_key_name(const char *secName, u32 keyIndex)
{
	return gf_cfg_get_key_name(gpac_global_config, secName, keyIndex);
}



GF_EXPORT
GF_Err gf_opts_discard_changes()
{
	return gf_cfg_discard_changes(gpac_global_config);
}

#include <gpac/main.h>
#include <gpac/filters.h>

GF_GPACArg GPAC_Args[] = {
 GF_DEF_ARG("log-file", "lf", "set output log file", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("log-clock", "lc", "log time in micro sec since start time of GPAC before each log line", NULL, NULL, GF_ARG_BOOL, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("log-utc", "lu", "log UTC time in ms before each log line", NULL, NULL, GF_ARG_BOOL, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("logs", NULL, "set log tools and levels."\
			"\n"\
			"You can independently log different tools involved in a session.\n"\
			"log_args is formatted as a ':'-separated list of toolX[:toolZ]@levelX\n"\
	        "levelX can be one of:\n"\
	        "\t        \"quiet\"      : skip logs\n"\
	        "\t        \"error\"      : logs only error messages\n"\
	        "\t        \"warning\"    : logs error+warning messages\n"\
	        "\t        \"info\"       : logs error+warning+info messages\n"\
	        "\t        \"debug\"      : logs all messages\n"\
	        "toolX can be one of:\n"\
	        "\t        \"core\"       : libgpac core\n"\
	        "\t        \"coding\"     : bitstream formats (audio, video, scene)\n"\
	        "\t        \"container\"  : container formats (ISO File, MPEG-2 TS, AVI, ...)\n"\
	        "\t        \"network\"    : network data exept RTP trafic\n"\
	        "\t        \"rtp\"        : rtp trafic\n"\
	        "\t        \"author\"     : authoring tools (hint, import, export)\n"\
	        "\t        \"sync\"       : terminal sync layer\n"\
	        "\t        \"codec\"      : terminal codec messages\n"\
	        "\t        \"parser\"     : scene parsers (svg, xmt, bt) and other\n"\
	        "\t        \"media\"      : terminal media object management\n"\
	        "\t        \"scene\"      : scene graph and scene manager\n"\
	        "\t        \"script\"     : scripting engine messages\n"\
	        "\t        \"interact\"   : interaction engine (events, scripts, etc)\n"\
	        "\t        \"smil\"       : SMIL timing engine\n"\
	        "\t        \"compose\"    : composition engine (2D, 3D, etc)\n"\
	        "\t        \"mmio\"       : Audio/Video HW I/O management\n"\
	        "\t        \"rti\"        : various run-time stats\n"\
	        "\t        \"cache\"      : HTTP cache subsystem\n"\
	        "\t        \"audio\"      : Audio renderer and mixers\n"\
	        "\t        \"mem\"        : GPAC memory tracker\n"\
	        "\t        \"dash\"       : HTTP streaming logs\n"\
	        "\t        \"module\"     : GPAC modules (av out, font engine, 2D rasterizer))\n"\
	        "\t        \"filter\"     : filters debugging\n"\
	        "\t        \"sched\"      : filter session scheduler debugging\n"\
	        "\t        \"mutex\"      : log all mutex calls\n"\
	        "\t        \"all\"        : all tools logged - other tools can be specified afterwards.\n"\
	        "The special keyword \"ncl\" can be set to disable color logs.\n"\
	        "The special keyword \"strict\" can be set to exit at first error.\n"\
	        "\tEX: -logs all@info:dash@debug:ncl moves all log to info level, dash to debug level and disable color logs."\
 			, NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_LOG),

 GF_DEF_ARG("noprog", NULL, "disables progress messages", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("quiet", NULL, "disables all messages, including errors", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_LOG),

 GF_DEF_ARG("strict-error", "se", "exits after the first error is reported", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("store-dir", NULL, "sets storage directory", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("mod-dirs", NULL, "sets module directories", NULL, NULL, GF_ARG_STRINGS, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("ifce", NULL, "sets default multicast interface through interface IP adress", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("lang", NULL, "sets preferred language", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("cfg", "opt", "sets configuration file value. The string parameter can be formatted as:\n"\
	        "\tsection:key=val: sets the key to a new value\n"\
	        "\tsection:key=null or section:key: removes the key\n"\
	        "\tsection:*=null: removes the section\n"\
			, NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("version", NULL, "sets to GPAC version, used to check config file refresh", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("64bits", NULL, "indicates if GPAC version is 64 bits, used to check config file refresh", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("mod-reload", NULL, "unload / reload module shared libs when no longer used", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("mobile-ip", NULL, "sets IP adress for mobileIP", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),

 GF_DEF_ARG("cache", NULL, "sets cache directory location", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("proxy-on", NULL, "Enables HTTP proxy", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("proxy-name", NULL, "sets HTTP proxy adress", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("proxy-port", NULL, "sets HTTP proxy port", "80", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("maxrate", NULL, "sets max HTTP download rate in bits per sec. 0 means unlimited", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("no-cache", NULL, "disables HTTP caching", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("offline-cache", NULL, "enables offline HTTP caching (no revalidation of existing resource in cache)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("clean-cache", NULL, "indicates if HTTP cache should be clean upon launch/exit", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("cache-size", NULL, "specifies cache size in bytes", "100M", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("head-timeout", NULL, "Sets HTTP head request timeout in milliseconds", "5000", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("req-timeout", NULL, "Sets HTTP/RTSP request timeout in milliseconds", "20000", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("broken-cert", NULL, "Enables accepting broken SSL certificates", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("user-agent", "ua", "sets user agent name for HTTP/RTSP", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("user-profileid", "upid", "sets user profile ID (through  \"X-UserProfileID\" entity header)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("user-profile", "up", "sets user profile filename. Content of file is appended as body to HEAD/GET requests", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("query-string", NULL, "inserts query string (without ?) to URL on requests", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),

 GF_DEF_ARG("dbg-edges", NULL, "logs edges status in filter graph before dijkstra resolution (for debug). Edges are logged as edge_source(status, weight, src_cap_idx, dst_cap_idx)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),

 GF_DEF_ARG("no-block", NULL, "disable blocking mode of filters", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-reg", NULL, "disable regulation (no sleep) in session", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("sched", NULL, "set scheduler mode. Possible modes are:\n"\
		"\tfree: uses lock-free queues except for task list (default)\n"\
		"\tlock: uses mutexes for queues when several threads\n"\
		"\tfreex: uses lock-free queues including for task lists (experimental)\n"\
		"\tflock: uses mutexes for queues even when no thread (debug mode)\n"\
		"\tdirect: uses no threads and direct dispatch of tasks whenever possible (debug mode)", "free", "free|lock|flock|freex|direct", GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("max-chain", NULL, "sets maximum chain length when resolving filter links.Default value covers for ([in ->] demux -> reframe -> decode -> encode -> reframe -> mux [-> out]. Filter chains loaded for adaptation (eg pixel format change) are loaded after the link resolution). Setting the value to 0 disables dynamic link resolution. You will have to specify the entire chain manually", "6", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),

 GF_DEF_ARG("threads", NULL, "set N extra thread for the session. -1 means use all available cores", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("blacklist", NULL, "blacklist the filters listed in the given string (comma-seperated list)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-graph-cache", NULL, "disable internal caching of filter graph connections. If disabled, the graph will be recomputed at each link resolution (less memory ungry but slower)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),

 GF_DEF_ARG("switch-vres", NULL, "selects smallest video resolution larger than scene size, otherwise use current video resolution", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("hwvmem", NULL, "specifies (2D renderer only) if main video backbuffer is always on hardware, always on system memory or selected by GPAC (default mode). Depending on the scene type, this may drastically change the playback speed", NULL, "auto|always|never", GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("vert-shader", NULL, "specifies path to vertex shader file", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_HIDE|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("frag-shader", NULL, "specifies path to vertex shader file", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_HIDE|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("pref-yuv4cc", NULL, "sets prefered YUV 4CC for overlays (used by DX only)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("yuv-overlay", NULL, "indicates YUV overlay is possible on the video card. Always overriden by video output module", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("offscreen-yuv", NULL, "indicates if offscreen yuv->rgb is enabled. can be set to false to force disabling", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("overlay-color-key", NULL, "indicates color to use for overlay keying, hex format", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-offscreen", NULL, "indicates openGL mode for offscreen rendering.\n"\
		"\tWindow: A hidden window is used to perform offscreen rendering. Depending on your video driver and X11 configuration, this may not work.\n"\
    	"\tVisibleWindow: A visible window is used to perform offscreen rendering. This can be usefull while debugging.\n"\
    	"\tPixmap: An X11 Pixmap is used to perform offscreen rendering. Depending on your video driver and X11 configuration, this may not work and can even crash the player.\n"\
    	"\tPBuffer: uses opengl PBuffers for drawing, not always supported.\n"\
 	, NULL, "Window|VisibleWindow|Pixmap|PBuffer", GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-bits-comp", NULL, "number of bits per color component in openGL", "8", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-bits-depth", NULL, "number of bits for depth buffer in openGL", "16", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-doublebuf", NULL, "enables openGL double buffering", "yes", NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("sdl-defer", NULL, "use defer rendering for SDL", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("no-colorkey", NULL, "disables color keying at the video output level", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("video-output", NULL, "indicates the name of the video output module to use", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("raster2d", NULL, "indicates the name of the 2D rasterizer module to use", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gapi-fbaccess", NULL, "sets GAPI access mode to the backbuffer", NULL, "base|raw|gx", GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),


 GF_DEF_ARG("audio-output", NULL, "indicates the name of the audio output module to use", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("alsa-devname", NULL, "sets ALSA dev name", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_AUDIO),
 GF_DEF_ARG("force-alsarate", NULL, "forces ALSA and OSS output sample rate", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_AUDIO),
 GF_DEF_ARG("ds-disable-notif", NULL, "disables DirectSound audio buffer notifications when supported", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_AUDIO),

 GF_DEF_ARG("font-reader", NULL, "indicates name of font reader module", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_TEXT),
 GF_DEF_ARG("font-dirs", NULL, "indicates comma-separated list of directories to scan for fonts", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_TEXT),
 GF_DEF_ARG("rescan-fonts", NULL, "indicates the font directory must be rescanned", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_TEXT),

 GF_DEF_ARG("rmt", NULL, "enables profiling through Remotery (https://github.com/Celtoys/Remotery). A copy of Remotery visualizer is in gpac/share/vis, usually installed in /usr/share/gpac/vis or Program Files/GPAC/vis", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-port", NULL, "sets remotery port", "17815", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-reuse", NULL, "have remotery reuse port", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-localhost", NULL, "make remotery only accepts localhost connection", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-sleep", NULL, "sets remotery sleep (ms) between server updates", "10", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-nmsg", NULL, "sets remotery number of messages per update", "10", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-qsize", NULL, "sets remotery message queue size in bytes", "131072", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-log", NULL, "redirects logs to remotery (experimental, usually not well handled by browser)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-ogl", NULL, "make remotery sample opengl calls", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 {0}
};

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
	if (!opt) return 0;
	char *sep = strchr(opt, 'k');
	if (sep) times=1000;
	else {
		sep = strchr(opt, 'K');
		if (sep) times=1000;
		else {
			sep = strchr(opt, 'm');
			if (sep) times=1000000;
			else {
				sep = strchr(opt, 'M');
				if (sep) times=1000000;
			}
		}
	}
	sscanf(opt, "%d", &val);
	val = atoi(opt);
	return val*times;
}

GF_EXPORT
Bool gf_sys_set_cfg_option(const char *opt_string)
{
	char *sep, *sep2, szSec[1024], szKey[1024], szVal[1024];
	sep = strchr(opt_string, ':');
	if (!sep) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[CoreArgs] Badly formatted option %s - expected Section:Name=Value\n", opt_string ) );
		return GF_FALSE;
	}
	{
		const size_t sepIdx = sep - opt_string;
		strncpy(szSec, opt_string, sepIdx);
		szSec[sepIdx] = 0;
	}
	sep ++;
	sep2 = strchr(sep, '=');
	if (!sep2) {
		gf_opts_set_key(szSec, sep, NULL);
		return  GF_TRUE;
	}

	const size_t sepIdx = sep2 - sep;
	strncpy(szKey, sep, sepIdx);
	szKey[sepIdx] = 0;
	strcpy(szVal, sep2+1);

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

	if (!strcmp(szSec, "core")) {
		if (!strcmp(szKey, "noprog") && (!strcmp(szVal, "yes")||!strcmp(szVal, "true")||!strcmp(szVal, "1")) ) {
			void gpac_disable_progress();

			gpac_disable_progress();
		}
	}
	return GF_TRUE;
}

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
		if (arg->altname) {
			char *alt = strstr(arg->altname, arg_name);
			if (alt) {
				char c = alt[strlen(arg_name)];
				if (!c || (c==' ')) break;
			}
		}
		arg = NULL;
	}
	if (!arg) return GF_FALSE;

	if (!strcmp(arg->name, "cfg")) {
		*consumed_next = GF_TRUE;
		if (! gf_sys_set_cfg_option(val)) *e = GF_BAD_PARAM;
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
		gf_opts_set_key("temp", arg->name, val);
	}
	return GF_TRUE;
}

GF_EXPORT
u32 gf_sys_is_gpac_arg(const char *arg_name)
{
	const GF_GPACArg *arg = NULL;
	u32 i=0;
	arg_name = arg_name+1;
	while (GPAC_Args[i].name) {
		arg = &GPAC_Args[i];
		i++;
		if (!strcmp(arg->name, arg_name)) break;
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
	return 2;
}


GF_EXPORT
void gf_sys_print_arg(const GF_GPACArg *arg)
{
	fprintf(stderr, "-%s", arg->name);
	if (arg->altname)
		fprintf(stderr, " (-%s)", arg->altname);

	switch (arg->type) {
	case GF_ARG_BOOL: fprintf(stderr, " [boolean]"); break;
	case GF_ARG_INT: fprintf(stderr, " [int]"); break;
	case GF_ARG_DOUBLE: fprintf(stderr, " [number]"); break;
	case GF_ARG_STRING: fprintf(stderr, " [string]"); break;
	case GF_ARG_STRINGS: fprintf(stderr, " [string list]"); break;
	default: break;
	}
	if (arg->val)
		fprintf(stderr, " (default %s)", arg->val);
	if (arg->values)
		fprintf(stderr, " (values %s)", arg->values);
	if (arg->desc)
		fprintf(stderr, ": %s", arg->desc);

	fprintf(stderr, "\n");
}


GF_EXPORT
void gf_sys_print_core_help(GF_FilterArgMode mode, u32 subsystem_flags)
{
	u32 i=0;
	const GF_GPACArg *args = gf_sys_get_options();

	fprintf(stderr, "These options can be set in the config file using section:key sytax core:OPTNAME=val, where OPTNAME is the option name without initial '-' character\n");
	
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
		gf_sys_print_arg(arg);
	}
}

#endif
