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
#define TEST_MODULE		"gm_dummy_in.dll"

#elif (defined(__DARWIN__) || defined(__APPLE__) )
#include <mach-o/dyld.h> /*for _NSGetExecutablePath */

#ifdef GPAC_IPHONE
#define TEST_MODULE     "osmo4ios"
#else
#define TEST_MODULE		"gm_dummy_in.dylib"
#endif
#define CFG_FILE_NAME	".gpacrc"

#else
#ifdef GPAC_CONFIG_LINUX
#include <unistd.h>
#endif
#define CFG_FILE_NAME	".gpacrc"
#define TEST_MODULE		"gm_dummy_in.so"
#endif


#ifdef GPAC_STATIC_MODULES
static Bool enum_mod_dir(void *cbck, char *item_name, char *item_path, GF_FileEnumInfo *file_info)
{
	if (!strnicmp(item_name, "gm_", 3)) {
		printf("Found %s\n", item_name);
		*(Bool *) cbck = GF_TRUE;
	}
	return GF_FALSE;
}
#endif

static Bool check_file_exists(char *name, char *path, char *outPath)
{
	char szPath[GF_MAX_PATH];
	FILE *f;

#ifdef GPAC_STATIC_MODULES
	if (!strcmp(name, TEST_MODULE)) {
		Bool found = GF_FALSE;
		gf_enum_directory(path, GF_FALSE, enum_mod_dir, &found, NULL);
		if (!found) return 0;
		if (outPath != path) strcpy(outPath, path);
		return 1;
	}
#endif
	sprintf(szPath, "%s%c%s", path, GF_PATH_SEPARATOR, name);
	f = fopen(szPath, "rb");
	if (!f) return 0;
	fclose(f);
	if (outPath != path) strcpy(outPath, path);
	return 1;
}

enum
{
	GF_PATH_APP,
	GF_PATH_CFG,
	GF_PATH_GUI,
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
	if (!strstr(file_path, "gpac")) {
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


	if (path_type==GF_PATH_APP) return 1;

	if (path_type==GF_PATH_GUI) {
		char *sep;
		strcat(file_path, "\\gui");
		if (check_file_exists("gui.bt", file_path, file_path)) return 1;
		sep = strstr(file_path, "\\bin\\");
		if (sep) {
			sep[0] = 0;
			strcat(file_path, "\\gui");
			if (check_file_exists("gui.bt", file_path, file_path)) return 1;
		}
		return 0;
	}
	/*modules are stored in the GPAC directory (should be changed to GPAC/modules)*/
	if (path_type==GF_PATH_MODULES) return 1;

	/*we are looking for the config file path - make sure it is writable*/
	assert(path_type == GF_PATH_CFG);

	strcpy(szPath, file_path);
	strcat(szPath, "\\gpaccfgtest.txt");
	f = gf_f64_open(szPath, "wb");
	if (f != NULL) {
		fclose(f);
		gf_delete_file(szPath);
		return 1;
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
	f = gf_f64_open(szPath, "wb");
	/*COMPLETE FAILURE*/
	if (!f) return 0;

	fclose(f);
	gf_delete_file(szPath);
	return 1;
#endif
}

/*FIXME - android initialization is a mess right now*/
#elif defined(GPAC_ANDROID)

static Bool get_default_install_path(char *file_path, u32 path_type)
{
	if (path_type==GF_PATH_APP) strcpy(file_path, "");
	else if (path_type==GF_PATH_CFG) strcpy(file_path, "");
	else if (path_type==GF_PATH_GUI) strcpy(file_path, "");
	else if (path_type==GF_PATH_MODULES) strcpy(file_path, "");
	return 1;
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
#ifdef GPAC_IPHONE
		char buf[PATH_MAX];
		char *res;
#endif
		if (!user_home) return 0;
#ifdef GPAC_IPHONE
		res = realpath(user_home, buf);
		if (res) {
			strcpy(file_path, buf);
		} else
#endif
			strcpy(file_path, user_home);
		if (file_path[strlen(file_path)-1] == '/') file_path[strlen(file_path)-1] = 0;
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
		size = readlink("/proc/self/exe", file_path, GF_MAX_PATH);
		if (size>0) {
			char *sep = strrchr(file_path, '/');
			if (sep) sep[0] = 0;
			return 1;
		}
#endif
		return 0;
	}


	/*locate the app*/
	if (!get_default_install_path(app_path, GF_PATH_APP)) return 0;

	/*installed or symlink on system, user user home directory*/
	if (!strnicmp(app_path, "/usr/", 5) || !strnicmp(app_path, "/opt/", 5)) {
		if (path_type==GF_PATH_GUI) {
			/*look in possible install dirs ...*/
			if (check_file_exists("gui.bt", "/usr/share/gpac/gui", file_path)) return 1;
			if (check_file_exists("gui.bt", "/usr/local/share/gpac/gui", file_path)) return 1;
			if (check_file_exists("gui.bt", "/opt/share/gpac/gui", file_path)) return 1;
			if (check_file_exists("gui.bt", "/opt/local/share/gpac/gui", file_path)) return 1;
		} else if (path_type==GF_PATH_MODULES) {
			/*look in possible install dirs ...*/
			if (check_file_exists(TEST_MODULE, "/usr/lib/gpac", file_path)) return 1;
			if (check_file_exists(TEST_MODULE, "/usr/local/lib/gpac", file_path)) return 1;
			if (check_file_exists(TEST_MODULE, "/opt/lib/gpac", file_path)) return 1;
			if (check_file_exists(TEST_MODULE, "/opt/local/lib/gpac", file_path)) return 1;
		}
	}

	if (path_type==GF_PATH_GUI) {
		if (get_default_install_path(app_path, GF_PATH_CFG)) {
			/*GUI not found, look in ~/.gpac/gui/ */
			strcat(app_path, "/.gpac/gui");
			if (check_file_exists("gui.bt", app_path, file_path)) return 1;
		}

		/*GUI not found, look in gpac distribution if any */
		if (get_default_install_path(app_path, GF_PATH_APP)) {
			char *sep = strstr(app_path, "/bin/gcc");
			if (sep) {
				sep[0] = 0;
				strcat(app_path, "/gui");
				if (check_file_exists("gui.bt", app_path, file_path)) return 1;
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
		}
		/*modules not found, look in ~/.gpac/modules/ */
		if (get_default_install_path(app_path, GF_PATH_CFG)) {
			strcpy(app_path, file_path);
			strcat(app_path, "/.gpac/modules");
			if (check_file_exists(TEST_MODULE, app_path, file_path)) return 1;
		}
		/*modules not found, failure*/
		return 0;
	}

	/*OSX way vs iPhone*/
	sep = strstr(app_path, ".app/");
	if (sep) sep[4] = 0;

	/*we are looking for .app install path, or GUI */
	if (path_type==GF_PATH_GUI) {
#ifndef GPAC_IPHONE
		strcat(app_path, "/Contents/MacOS/gui");
		if (check_file_exists("gui.bt", app_path, file_path)) return 1;
#else /*iOS: for now, everything is set flat within the package*/
		/*iOS app is distributed with embedded GUI*/
		get_default_install_path(app_path, GF_PATH_APP);
		strcat(app_path, "/gui");
		if (check_file_exists("gui_old.bt", app_path, file_path)) return 1;
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


static GF_Config *create_default_config(char *file_path)
{
	FILE *f;
	GF_Config *cfg;
	char *cache_dir;
	char szPath[GF_MAX_PATH];
	char gui_path[GF_MAX_PATH];

	if (! get_default_install_path(file_path, GF_PATH_CFG)) {
		gf_delete_file(szPath);
		return NULL;
	}
	/*Create the config file*/
	sprintf(szPath, "%s%c%s", file_path, GF_PATH_SEPARATOR, CFG_FILE_NAME);
	fprintf(stderr, "Trying to create config file: %s", szPath);
	f = gf_f64_open(szPath, "wt");
	if (!f) return NULL;
	fclose(f);

	if (! get_default_install_path(szPath, GF_PATH_MODULES)) {
		gf_delete_file(szPath);
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("[Core] default modules not found\n"));
		return NULL;
	}

	cfg = gf_cfg_new(file_path, CFG_FILE_NAME);
	if (!cfg) return NULL;

	gf_cfg_set_key(cfg, "General", "ModulesDirectory", szPath);

	/*get default temporary directoy */
	cache_dir = gf_get_default_cache_directory();
	if (cache_dir) {
		gf_cfg_set_key(cfg, "General", "CacheDirectory", cache_dir);
		gf_free(cache_dir);
	}
	gf_cfg_set_key(cfg, "Compositor", "Raster2D", "GPAC 2D Raster");
	gf_cfg_set_key(cfg, "Audio", "ForceConfig", "yes");
	gf_cfg_set_key(cfg, "Audio", "NumBuffers", "2");
	gf_cfg_set_key(cfg, "Audio", "TotalDuration", "120");
	gf_cfg_set_key(cfg, "Audio", "DisableNotification", "no");

	/*Setup font engine to FreeType by default, and locate TrueType font directory on the system*/
	gf_cfg_set_key(cfg, "FontEngine", "FontReader", "FreeType Font Reader");
	gf_cfg_set_key(cfg, "FontEngine", "RescanFonts", "yes");


#if defined(_WIN32_WCE)
	/*FIXME - is this true on all WinCE systems??*/
	strcpy(szPath, "\\Windows");
#elif defined(WIN32)
	GetWindowsDirectory((char*)szPath, MAX_PATH);
	if (szPath[strlen((char*)szPath)-1] != '\\') strcat((char*)szPath, "\\");
	strcat((char *)szPath, "Fonts");
#elif defined(__APPLE__)

#ifdef GPAC_IPHONE
	strcpy(szPath, "/System/Library/Fonts/Cache");
#else
	strcpy(szPath, "/Library/Fonts");
#endif

#else
	strcpy(szPath, "/usr/share/fonts/truetype/");
#endif
	gf_cfg_set_key(cfg, "FontEngine", "FontDirectory", szPath);

	gf_cfg_set_key(cfg, "Downloader", "CleanCache", "yes");
	gf_cfg_set_key(cfg, "Compositor", "AntiAlias", "All");
	gf_cfg_set_key(cfg, "Compositor", "FrameRate", "30.0");
	/*use power-of-2 emulation in OpenGL if no rectangular texture extension*/
	gf_cfg_set_key(cfg, "Compositor", "EmulatePOW2", "yes");
	gf_cfg_set_key(cfg, "Compositor", "ScalableZoom", "yes");

#if defined(_WIN32_WCE)
	gf_cfg_set_key(cfg, "Video", "DriverName", "GAPI Video Output");
#elif defined(WIN32)
	gf_cfg_set_key(cfg, "Video", "DriverName", "DirectX Video Output");
#elif defined(__DARWIN__) || defined(__APPLE__)
	gf_cfg_set_key(cfg, "Video", "DriverName", "SDL Video Output");
#else
	gf_cfg_set_key(cfg, "Video", "DriverName", "X11 Video Output");
	gf_cfg_set_key(cfg, "Audio", "DriverName", "SDL Audio Output");
#endif
#ifdef GPAC_IPHONE
	gf_cfg_set_key(cfg, "Compositor", "DisableGLUScale", "yes");
#endif

	gf_cfg_set_key(cfg, "Video", "SwitchResolution", "no");
	gf_cfg_set_key(cfg, "Video", "HardwareMemory", "Auto");
	gf_cfg_set_key(cfg, "Network", "AutoReconfigUDP", "yes");
	gf_cfg_set_key(cfg, "Network", "UDPTimeout", "10000");
	gf_cfg_set_key(cfg, "Network", "BufferLength", "3000");


	/*locate GUI*/
	if ( get_default_install_path(szPath, GF_PATH_GUI) ) {
		sprintf(gui_path, "%s%cgui.bt", szPath, GF_PATH_SEPARATOR);
		f = fopen(gui_path, "rt");
		if (f) {
			fclose(f);
			gf_cfg_set_key(cfg, "General", "StartupFile", gui_path);
		}
	}

	/*store and reload*/
	gf_cfg_del(cfg);
	return gf_cfg_new(file_path, CFG_FILE_NAME);
}

/*check if modules directory has changed in the config file
*/
static void check_modules_dir(GF_Config *cfg)
{
	char path[GF_MAX_PATH];
	const char *opt;

	if ( get_default_install_path(path, GF_PATH_MODULES) ) {
		opt = gf_cfg_get_key(cfg, "General", "ModulesDirectory");
		//for OSX, we can have an install in /usr/... and an install in /Applications/Osmo4.app - always change
#if defined(__DARWIN__) || defined(__APPLE__)
		if (!opt || strcmp(opt, path))
			gf_cfg_set_key(cfg, "General", "ModulesDirectory", path);
#else

		//otherwise only check we didn't switch between a 64 bit version and a 32 bit version
		if (!opt) {
			gf_cfg_set_key(cfg, "General", "ModulesDirectory", path);
		} else  {
			Bool erase_modules_dir = 0;
			const char *opt64 = gf_cfg_get_key(cfg, "Systems", "64bits");
			if (!opt64) {
				//first run or old versions, erase
				erase_modules_dir = 1;
			} else if (!strcmp(opt64, "yes") ) {
#ifndef GPAC_64_BITS
				erase_modules_dir = 1;
#endif
			} else {
#ifdef GPAC_64_BITS
				erase_modules_dir = 1;
#endif
			}

#ifdef GPAC_64_BITS
			opt64 = "yes";
#else
			opt64 = "no";
#endif
			gf_cfg_set_key(cfg, "Systems", "64bits", opt64);

			if (erase_modules_dir) {
				gf_cfg_set_key(cfg, "General", "ModulesDirectory", path);
			}
		}
#endif
	}

	/*if startup file was disabled, do not attempt to correct it*/
	if (gf_cfg_get_key(cfg, "General", "StartupFile")==NULL) return;

	if ( get_default_install_path(path, GF_PATH_GUI) ) {
		opt = gf_cfg_get_key(cfg, "General", "StartupFile");
		if (strstr(opt, "gui.bt") && strcmp(opt, path)) {
#if defined(__DARWIN__) || defined(__APPLE__)
			strcat(path, "/gui.bt");
			gf_cfg_set_key(cfg, "General", "StartupFile", path);
#endif
		}
	}
}

GF_EXPORT
GF_Config *gf_cfg_init(const char *file, Bool *new_cfg)
{
	GF_Config *cfg;
	char szPath[GF_MAX_PATH];

	if (new_cfg) *new_cfg = 0;

	if (file) {
		cfg = gf_cfg_new(NULL, file);
		/*force creation of a new config*/
		if (!cfg) {
			FILE *fcfg = fopen(file, "wt");
			if (fcfg) {
				fclose(fcfg);
				cfg = gf_cfg_new(NULL, file);
				if (new_cfg) *new_cfg = 1;
			}
		}
		if (cfg) {
			check_modules_dir(cfg);
			return cfg;
		}
	}

	if (!get_default_install_path(szPath, GF_PATH_CFG)) {
		fprintf(stderr, "Fatal error: Cannot create a configuration file in application or user home directory - no write access\n");
		return NULL;
	}
	cfg = gf_cfg_new(szPath, CFG_FILE_NAME);
	if (!cfg) {
		fprintf(stderr, "GPAC config file %s not found in %s - creating new file\n", CFG_FILE_NAME, szPath);
		cfg = create_default_config(szPath);
	}

	if (!cfg) {
		fprintf(stderr, "Cannot create config file %s in %s directory\n", CFG_FILE_NAME, szPath);
		return NULL;
	}

	fprintf(stderr, "Using config file in %s directory\n", szPath);

	check_modules_dir(cfg);

	if (new_cfg) *new_cfg = 1;
	return cfg;
}

#endif
