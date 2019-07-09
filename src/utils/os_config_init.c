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
	gf_cfg_set_key(cfg, "core", "cache", cache_dir);
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
		gf_cfg_set_key(cfg, "General", "StartupFile", path);
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

void gf_uninit_global_config(Bool discard_config)
{
	if (gpac_global_config) {
		if (discard_config) gf_cfg_discard_changes(gpac_global_config);
		gf_cfg_del(gpac_global_config);
		gpac_global_config = NULL;
		gf_modules_del();
	}
}

void gf_cfg_load_restrict()
{
	GF_Err gf_cfg_set_key_internal(GF_Config *iniFile, const char *secName, const char *keyName, const char *keyValue, Bool is_restrict);
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
	const char *gf_cfg_get_key_internal(GF_Config *iniFile, const char *secName, const char *keyName, Bool restricted_only);
	if (!gpac_global_config) return NULL;
	return gf_cfg_get_key_internal(gpac_global_config, secName, keyName, GF_TRUE);
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

#include <gpac/main.h>

GF_GPACArg GPAC_Args[] = {
 GF_DEF_ARG("noprog", NULL, "disable progress messages", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("quiet", NULL, "disable all messages, including errors", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("log-file", "lf", "set output log file", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("log-clock", "lc", "log time in micro sec since start time of GPAC before each log line", NULL, NULL, GF_ARG_BOOL, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("log-utc", "lu", "log UTC time in ms before each log line", NULL, NULL, GF_ARG_BOOL, GF_ARG_SUBSYS_LOG),
 GF_DEF_ARG("logs", NULL, "set log tools and levels.  \n"\
			"  \n"\
			"You can independently log different tools involved in a session.\n"\
			"log_args is formatted as a ':'-separated list of `toolX[:toolZ]@levelX`\n"\
	        "`levelX` can be one of:\n"\
	        "- quiet: skip logs\n"\
	        "- error: logs only error messages\n"\
	        "- warning: logs error+warning messages\n"\
	        "- info: logs error+warning+info messages\n"\
	        "- debug: logs all messages\n"\
	        "`toolX` can be one of:\n"\
	        "- core: libgpac core\n"\
	        "- coding: bitstream formats (audio, video, scene)\n"\
	        "- container: container formats (ISO File, MPEG-2 TS, AVI, ...)\n"\
	        "- network: network data exept RTP trafic\n"\
	        "- rtp: rtp trafic\n"\
	        "- author: authoring tools (hint, import, export)\n"\
	        "- sync: terminal sync layer\n"\
	        "- codec: terminal codec messages\n"\
	        "- parser: scene parsers (svg, xmt, bt) and other\n"\
	        "- media: terminal media object management\n"\
	        "- scene: scene graph and scene manager\n"\
	        "- script: scripting engine messages\n"\
	        "- interact: interaction engine (events, scripts, etc)\n"\
	        "- smil: SMIL timing engine\n"\
	        "- compose: composition engine (2D, 3D, etc)\n"\
	        "- mmio: Audio/Video HW I/O management\n"\
	        "- rti: various run-time stats\n"\
	        "- cache: HTTP cache subsystem\n"\
	        "- audio: Audio renderer and mixers\n"\
	        "- mem: GPAC memory tracker\n"\
	        "- dash: HTTP streaming logs\n"\
	        "- module: GPAC modules (av out, font engine, 2D rasterizer)\n"\
	        "- filter: filters debugging\n"\
	        "- sched: filter session scheduler debugging\n"\
	        "- mutex: log all mutex calls\n"\
	        "- all: all tools logged - other tools can be specified afterwards.  \n"\
	        "The special keyword `ncl` can be set to disable color logs.  \n"\
	        "The special keyword `strict` can be set to exit at first error.  \n"\
	        "EX -logs all@info:dash@debug:ncl\n"\
			"This moves all log to info level, dash to debug level and disable color logs"\
 			, NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_LOG),

 GF_DEF_ARG("strict-error", "se", "exit after the first error is reported", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("store-dir", NULL, "set storage directory", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("mod-dirs", NULL, "set module directories", NULL, NULL, GF_ARG_STRINGS, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("ifce", NULL, "set default multicast interface through interface IP address", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("lang", NULL, "set preferred language", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("cfg", "opt", "set configuration file value. The string parameter can be formatted as:\n"\
	        "- `section:key=val`: set the key to a new value\n"\
	        "- `section:key=null`, `section:key`: remove the key\n"\
	        "- `section:*=null`: remove the section"\
			, NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("no-save", NULL, "discard any changes made to the config file upon exit", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("version", NULL, "set to GPAC version, used to check config file refresh", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("mod-reload", NULL, "unload / reload module shared libs when no longer used", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("for-test", NULL, "disable all creation/modif dates and GPAC versions in files", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_CORE),
 GF_DEF_ARG("cache", NULL, "cache directory location", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("proxy-on", NULL, "enable HTTP proxy", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("proxy-name", NULL, "set HTTP proxy address", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("proxy-port", NULL, "set HTTP proxy port", "80", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("maxrate", NULL, "set max HTTP download rate in bits per sec. 0 means unlimited", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("no-cache", NULL, "disable HTTP caching", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("offline-cache", NULL, "enable offline HTTP caching (no revalidation of existing resource in cache)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("clean-cache", NULL, "indicate if HTTP cache should be clean upon launch/exit", NULL, NULL, GF_ARG_STRING, GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("cache-size", NULL, "specify cache size in bytes", "100M", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("head-timeout", NULL, "set HTTP head request timeout in milliseconds", "5000", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("req-timeout", NULL, "set HTTP/RTSP request timeout in milliseconds", "20000", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("broken-cert", NULL, "enable accepting broken SSL certificates", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("user-agent", "ua", "set user agent name for HTTP/RTSP", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("user-profileid", "upid", "set user profile ID (through **X-UserProfileID** entity header)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("user-profile", "up", "set user profile filename. Content of file is appended as body to HEAD/GET requests", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),
 GF_DEF_ARG("query-string", NULL, "insert query string (without `?`) to URL on requests", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_HTTP),

 GF_DEF_ARG("dbg-edges", NULL, "log edges status in filter graph before dijkstra resolution (for debug). Edges are logged as edge_source(status, weight, src_cap_idx, dst_cap_idx)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),

 GF_DEF_ARG("no-block", NULL, "disable blocking mode of filters", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-reg", NULL, "disable regulation (no sleep) in session", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-reassign", NULL, "disable source filter reassignment in pid graph resolution", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("sched", NULL, "set scheduler mode\n"\
		"- free: lock-free queues except for task list (default)\n"\
		"- lock: mutexes for queues when several threads\n"\
		"- freex: lock-free queues including for task lists (experimental)\n"\
		"- flock: mutexes for queues even when no thread (debug mode)\n"\
		"- direct: no threads and direct dispatch of tasks whenever possible (debug mode)", "free", "free|lock|flock|freex|direct", GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("max-chain", NULL, "set maximum chain length when resolving filter links. Default value covers for __[ in -> ] demux -> reframe -> decode -> encode -> reframe -> mux [ -> out]__. Filter chains loaded for adaptation (eg pixel format change) are loaded after the link resolution. Setting the value to 0 disables dynamic link resolution. You will have to specify the entire chain manually", "6", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("max-sleep", NULL, "set maximum sleep time slot in milliseconds when regulation is enabled", "50", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),

 GF_DEF_ARG("threads", NULL, "set N extra thread for the session. -1 means use all available cores", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-probe", NULL, "disable data probing on sources and relies on extension (faster load but more error-prone)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-argchk", NULL, "disable tracking of argument usage (all arguments will be considered as used)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("blacklist", NULL, "blacklist the filters listed in the given string (comma-seperated list)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-graph-cache", NULL, "disable internal caching of filter graph connections. If disabled, the graph will be recomputed at each link resolution (lower memory usage but slower)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),
 GF_DEF_ARG("no-reservoir", NULL, "disable memory recycling for packets and properties. This uses much less memory but stresses the system memory allocator much more", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_FILTERS),

 GF_DEF_ARG("switch-vres", NULL, "select smallest video resolution larger than scene size, otherwise use current video resolution", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("hwvmem", NULL, "specify (2D rendering only) memory type of main video backbuffer. Depending on the scene type, this may drastically change the playback speed.\n"
 "- always: always on hardware\n"
 "- never: always on system memory\n"
 "- auto: selected by GPAC based on content type (graphics or video)", "auto", "auto|always|never", GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("pref-yuv4cc", NULL, "set prefered YUV 4CC for overlays (used by DirectX only)", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("yuv-overlay", NULL, "indicate YUV overlay is possible on the video card. Always overriden by video output module", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_HIDE|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("offscreen-yuv", NULL, "indicate if offscreen yuv->rgb is enabled. can be set to false to force disabling", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("overlay-color-key", NULL, "color to use for overlay keying, hex format", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-offscreen", NULL, "openGL mode for offscreen rendering (will soon be deprecated).\n"\
		"- Window: a hidden window is used to perform offscreen rendering. Depending on your video driver and X11 configuration, this may not work\n"\
    	"- VisibleWindow: a visible window is used to perform offscreen rendering. This can be usefull while debugging\n"\
    	"- Pixmap: an X11 Pixmap is used to perform offscreen rendering. Depending on your video driver and X11 configuration, this may not work and can even crash the player\n"\
    	"- PBuffer: use opengl PBuffers for drawing, not always supported"\
 	, "Window", "Window|VisibleWindow|Pixmap|PBuffer", GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-bits-comp", NULL, "number of bits per color component in openGL", "8", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-bits-depth", NULL, "number of bits for depth buffer in openGL", "16", NULL, GF_ARG_INT, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("gl-doublebuf", NULL, "enable openGL double buffering", "yes", NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("sdl-defer", NULL, "use defer rendering for SDL", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("no-colorkey", NULL, "disable color keying at the video output level", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("video-output", NULL, "indicate the name of the video output module to use", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("audio-output", NULL, "indicate the name of the audio output module to use", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_VIDEO),
 GF_DEF_ARG("alsa-devname", NULL, "set ALSA dev name", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_AUDIO),
 GF_DEF_ARG("force-alsarate", NULL, "force ALSA and OSS output sample rate", NULL, NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_AUDIO),
 GF_DEF_ARG("ds-disable-notif", NULL, "disable DirectSound audio buffer notifications when supported", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_AUDIO),
 GF_DEF_ARG("font-reader", NULL, "indicate name of font reader module", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_TEXT),
 GF_DEF_ARG("font-dirs", NULL, "indicate comma-separated list of directories to scan for fonts", NULL, NULL, GF_ARG_STRING, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_TEXT),
 GF_DEF_ARG("rescan-fonts", NULL, "indicate the font directory must be rescanned", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_ADVANCED|GF_ARG_SUBSYS_TEXT),
 GF_DEF_ARG("rmt", NULL, "enable profiling through [Remotery](https://github.com/Celtoys/Remotery). A copy of Remotery visualizer is in gpac/share/vis, usually installed in __/usr/share/gpac/vis__ or __Program Files/GPAC/vis__", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-port", NULL, "set remotery port", "17815", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-reuse", NULL, "allow remotery to reuse port", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-localhost", NULL, "make remotery only accepts localhost connection", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-sleep", NULL, "set remotery sleep (ms) between server updates", "10", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-nmsg", NULL, "set remotery number of messages per update", "10", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-qsize", NULL, "set remotery message queue size in bytes", "131072", NULL, GF_ARG_INT, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-log", NULL, "redirect logs to remotery (experimental, usually not well handled by browser)", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
 GF_DEF_ARG("rmt-ogl", NULL, "make remotery sample opengl calls", NULL, NULL, GF_ARG_BOOL, GF_ARG_HINT_EXPERT|GF_ARG_SUBSYS_RMT),
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
		sep = strchr(arg->description, ' ');
		if (sep) sep--;
		if (sep[0] == 's') {
			fprintf(stderr, "\nWARNING: arg %s bad description format \"%s\", should use infintive\n", arg->name, arg->description);
			exit(1);
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
		fprintf(helpout, ".TP\n.B \\-%s", arg_name ? arg_name : arg->name);
	}
	else if (gen_doc==1) {
		gf_sys_format_help(helpout, flags, "<a id=\"%s\">", arg_name ? arg_name : arg->name);
		gf_sys_format_help(helpout, flags | GF_PRINTARG_HIGHLIGHT_FIRST, "-%s", arg_name ? arg_name : arg->name);
		gf_sys_format_help(helpout, flags, "</a>");
	} else {
		gf_sys_format_help(helpout, flags | GF_PRINTARG_HIGHLIGHT_FIRST, "-%s", arg_name ? arg_name : arg->name);
	}
	if (arg->altname) {
		gf_sys_format_help(helpout, flags, " ");
		gf_sys_format_help(helpout, flags | GF_PRINTARG_HIGHLIGHT_FIRST, "-%s", arg->altname);
	}
	if (syntax) {
		gf_sys_format_help(helpout, flags, " %s", syntax);
		gf_free(arg_name);
	}

	if (arg->type==GF_ARG_INT && arg->values && strchr(arg->values, '|')) {
		gf_sys_format_help(helpout, flags, " (Enum");
		if (arg->val)
			gf_sys_format_help(helpout, flags, ", default: **%s**", arg->val);
		gf_sys_format_help(helpout, flags, ")");
	} else {
		gf_sys_format_help(helpout, flags, " (");
		switch (arg->type) {
		case GF_ARG_BOOL: gf_sys_format_help(helpout, flags, "boolean"); break;
		case GF_ARG_INT: gf_sys_format_help(helpout, flags, "int"); break;
		case GF_ARG_DOUBLE: gf_sys_format_help(helpout, flags, "number"); break;
		case GF_ARG_STRING: gf_sys_format_help(helpout, flags, "string"); break;
		case GF_ARG_STRINGS: gf_sys_format_help(helpout, flags, "string list"); break;
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


#define LINE_OFFSET_DESCR 40

static char *help_buf = NULL;
static u32 help_buf_size=0;

void gf_sys_cleanup_help()
{
	if (help_buf) gf_free(help_buf);
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

GF_EXPORT
void gf_sys_format_help(FILE *helpout, u32 flags, const char *fmt, ...)
{
	char *line;
	u32 len;
	va_list vlist;
	u32 gen_doc = 0;
	u32 is_app_opts = 0;
	if (flags & GF_PRINTARG_MD)
		gen_doc = 1;
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
				header_string = ".P\n.B\n";
			}

			console_code = GF_CONSOLE_MAGENTA;
			line_before = GF_TRUE;
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
					} else if (!strncmp(link, "CORE", 3)) {
						fprintf(helpout, "[-%s](core_options/#%s)", line, line);
						line_pos+=7 + 2* (u32)strlen(line) + (u32)strlen("core_options");
						line_pos+=7 + 2*(u32)strlen(line) + (u32)strlen("core_options");
					} else if (!strncmp(link, "CFG", 3)) {
						fprintf(helpout, "[-%s](core_config/#%s)", line, line);
						line_pos+=7 + 2*(u32)strlen(line) + (u32)strlen("core_config");
					} else if (!strncmp(link, "MP4B_GEN", 3)) {
						fprintf(helpout, "[-%s](mp4box-gen-opts/#%s)", line, line);
						line_pos+=7 + 2* (u32)strlen(line) + (u32)strlen("mp4box-gen-opts");
					} else if (strlen(link)) {
						fprintf(helpout, "[-%s](%s/#%s)", line, link, line);
						line_pos+=7 + 2* (u32)strlen(line) + (u32)strlen(link);
					} else if (is_app_opts || !strcmp(line, "i") || !strcmp(line, "o") || !strcmp(line, "h")) {
						fprintf(helpout, "[-%s](#%s)", line, line);
						line_pos+=5 + 2* (u32)strlen(line) + (u32)strlen(link);
					} else {
						//this is a filter opt, don't print '-'
						fprintf(helpout, "[%s](#%s)", line, line);
						line_pos+=4 + 2* (u32)strlen(line) + (u32)strlen(link);
					}
				} else {
					if (gen_doc==2)
						fprintf(helpout, ".I ");

					if (!strncmp(link, "GPAC", 4)
						|| !strncmp(link, "LOG", 3)
						|| !strncmp(link, "CORE", 3)
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

#endif
