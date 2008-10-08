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

#include <gpac/network.h>

/* the length of the URL separator ("://" || "|//") */
#define URL_SEP_LENGTH	3

/* our supported protocol types */
enum
{
	/*absolute path to file*/
	GF_URL_TYPE_FILE = 0,
	/*relative URL*/
	GF_URL_TYPE_RELATIVE ,
	/*any other URL*/
	GF_URL_TYPE_ANY
};

/*resolve the protocol type, for a std URL: http:// or ftp:// ...*/
static u32 URL_GetProtocolType(const char *pathName)
{
	char *begin;
	if (!pathName) return GF_URL_TYPE_ANY;

	if ((pathName[0] == '/') || (pathName[0] == '\\') 
		|| (pathName[1] == ':') 
		|| ((pathName[0] == ':') && (pathName[1] == ':'))
		) return GF_URL_TYPE_FILE;

	begin = strstr(pathName, "://");
	if (!begin) begin = strstr(pathName, "|//"); 
	if (!begin) return GF_URL_TYPE_RELATIVE;
	if (!strnicmp(pathName, "file", 4)) return GF_URL_TYPE_FILE;
	return GF_URL_TYPE_ANY;
}

/*gets protocol type*/
Bool gf_url_is_local(const char *pathName)
{
	u32 mode = URL_GetProtocolType(pathName);
	return (mode==GF_URL_TYPE_ANY) ? 0 : 1;
}

char *gf_url_get_absolute_path(const char *pathName, const char *parentPath)
{
	u32 prot_type = URL_GetProtocolType(pathName);

	/*abs path name*/
	if (prot_type == GF_URL_TYPE_FILE) {
	  /*abs path*/
		if (!strstr(pathName, "://") && !strstr(pathName, "|//")) return strdup(pathName);
		pathName += 6;
		/*not sure if "file:///C:\..." is std, but let's handle it anyway*/
		if ((pathName[0]=='/') && (pathName[2]==':')) pathName += 1;
		return strdup(pathName);
	}
	if (prot_type==GF_URL_TYPE_ANY) return NULL;
	if (!parentPath) return strdup(pathName);

	/*try with the parent URL*/
	prot_type = URL_GetProtocolType(parentPath);
	/*if abs parent path concatenate*/
	if (prot_type == GF_URL_TYPE_FILE) return gf_url_concatenate(parentPath, pathName);
	if (prot_type != GF_URL_TYPE_RELATIVE) return NULL;
	/*if we are here, parentPath is also relative... return the original PathName*/
	return strdup(pathName);
}


char *gf_url_concatenate(const char *parentName, const char *pathName)
{
	u32 pathSepCount, i, prot_type;
	char psep;
	char *outPath, *name;
	char tmp[GF_MAX_PATH];

	if (!pathName || !parentName) return NULL;

	if ( (strlen(parentName) > GF_MAX_PATH) || (strlen(pathName) > GF_MAX_PATH) ) return NULL;

	prot_type = URL_GetProtocolType(pathName);
	if (prot_type != GF_URL_TYPE_RELATIVE) {
		outPath = strdup(pathName);
		goto check_spaces;
	}

	pathSepCount = 0;
	name = NULL;
	if (pathName[0] == '.') {
		if (!strcmp(pathName, "..")) {
			pathSepCount = 1;
			name = "";
		}
		for (i = 0; i< strlen(pathName) - 2; i++) {
			/*current dir*/
			if ( (pathName[i] == '.') 
				&& ( (pathName[i+1] == GF_PATH_SEPARATOR) || (pathName[i+1] == '/') ) ) {
				i++;
				continue;
			}
			/*parent dir*/
			if ( (pathName[i] == '.') && (pathName[i+1] == '.') 
				&& ( (pathName[i+2] == GF_PATH_SEPARATOR) || (pathName[i+2] == '/') )
				) {
				pathSepCount ++;
				i+=2;
				name = (char *) &pathName[i+1];
			} else {
				name = (char *) &pathName[i];
				break;
			}
		}
	}
	if (!name) name = (char *) pathName;

	strcpy(tmp, parentName);
	for (i = strlen(parentName); i > 0; i--) {
		//break our path at each separator
		if ((parentName[i-1] == GF_PATH_SEPARATOR) || (parentName[i-1] == '/'))  {
			tmp[i-1] = 0;
			if (!pathSepCount) break;
			pathSepCount--;
		}
	}
	//if i==0, the parent path was relative, just return the pathName
	if (!i) {
		outPath = strdup(pathName);
		goto check_spaces;
	}

	prot_type = URL_GetProtocolType(parentName);
	psep = (prot_type == GF_URL_TYPE_FILE) ? GF_PATH_SEPARATOR : '/';

	outPath = (char *) malloc(strlen(tmp) + strlen(name) + 2);
	sprintf(outPath, "%s%c%s", tmp, psep, name);

	/*cleanup paths sep for win32*/
//	if ((prot_type == GF_URL_TYPE_FILE) && (GF_PATH_SEPARATOR != '/')) {
		for (i = 0; i<strlen(outPath); i++) 
//			if (outPath[i]=='/') outPath[i] = GF_PATH_SEPARATOR;
			if (outPath[i]=='\\') outPath[i] = '/';
//	}

check_spaces:
	while (1) {
		char *str = strstr(outPath, "%20");
		if (!str) break;
		str[0] = ' ';
		memmove(str+1, str+3, strlen(str)-2);
	}
	return outPath;
}

GF_EXPORT
void gf_url_to_fs_path(char *sURL)
{
	if (!strnicmp(sURL, "file://", 7)) {
		/*file:///C:\ scheme*/
		if ((sURL[7]=='/') && (sURL[9]==':')) {
			memmove(sURL, sURL+8, strlen(sURL)-7);
		} else {
			memmove(sURL, sURL+7, strlen(sURL)-6);
		}
	}

	while (1) {
		char *sep = strstr(sURL, "%20");
		if (!sep) break;
		sep[0] = ' ';
		memmove(sep+1, sep+3, strlen(sep)-2);
	}
}

