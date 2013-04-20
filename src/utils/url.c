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
	GF_URL_TYPE_ANY,
	/*BLOB URL*/
	GF_URL_TYPE_BLOB
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

    if (!strncmp(pathName, "blob:", 5)) return GF_URL_TYPE_BLOB;

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
		if (!strstr(pathName, "://") && !strstr(pathName, "|//")) return gf_strdup(pathName);
		pathName += 6;
		/*not sure if "file:///C:\..." is std, but let's handle it anyway*/
		if ((pathName[0]=='/') && (pathName[2]==':')) pathName += 1;
		return gf_strdup(pathName);
	}
	if (prot_type==GF_URL_TYPE_ANY) return NULL;
	if (!parentPath) return gf_strdup(pathName);

	/*try with the parent URL*/
	prot_type = URL_GetProtocolType(parentPath);
	/*if abs parent path concatenate*/
	if (prot_type == GF_URL_TYPE_FILE) return gf_url_concatenate(parentPath, pathName);
	if (prot_type != GF_URL_TYPE_RELATIVE) return NULL;
	/*if we are here, parentPath is also relative... return the original PathName*/
	return gf_strdup(pathName);
}

GF_EXPORT
char *gf_url_concatenate(const char *parentName, const char *pathName)
{
	u32 pathSepCount, i, prot_type;
	char *outPath, *name, *rad;
	char tmp[GF_MAX_PATH];

	if (!pathName && !parentName) return NULL;
	if (!pathName) return gf_strdup(parentName);
	if (!parentName) return gf_strdup(pathName);

	if ( (strlen(parentName) > GF_MAX_PATH) || (strlen(pathName) > GF_MAX_PATH) ) return NULL;

	prot_type = URL_GetProtocolType(pathName);
	if (prot_type != GF_URL_TYPE_RELATIVE) {
		char *sep = NULL;
		if (pathName[0]=='/') sep = strstr(parentName, "://");
		if (sep) sep = strchr(sep+3, '/');
		if (sep) {
			u32 len;
			sep[0] = 0;
			len = (u32) strlen(parentName);
			outPath = (char*)gf_malloc(sizeof(char)*(len+1+strlen(pathName)));
			strcpy(outPath, parentName);
			strcat(outPath, pathName);
			sep[0] = '/';
		} else {
			outPath = gf_strdup(pathName);
		}
		goto check_spaces;
	}

	/*old upnp addressing a la Platinum*/
	rad = strstr(parentName, "%3fpath=");
	if (!rad) rad = strstr(parentName, "%3Fpath=");
	if (!rad) rad = strstr(parentName, "?path=");
	if (rad) {
		char *the_path;
		rad = strchr(rad, '=');
		rad[0] = 0;
		the_path = gf_strdup(rad+1);
		i=0;
		while (1) {
			if (the_path[i]==0) break;
			if (!strnicmp(the_path+i, "%5c", 3) || !strnicmp(the_path+i, "%2f", 3) ) {
				the_path[i] = '/';
				memmove(the_path+i+1, the_path+i+3, strlen(the_path+i+3)+1);
			}
			else if (!strnicmp(the_path+i, "%05c", 4) || !strnicmp(the_path+i, "%02f", 4) ) {
				the_path[i] = '/';
				memmove(the_path+i+1, the_path+i+4, strlen(the_path+i+4)+1);
			}
			i++;
		}
		name = gf_url_concatenate(the_path, pathName);
		outPath = gf_malloc(strlen(parentName) + strlen(name) + 2);
		sprintf(outPath, "%s=%s", parentName, name);
		rad[0] = '=';
		gf_free(name);
		gf_free(the_path);
		return outPath;
	}

	/*rewrite path to use / not % encoding*/
	rad = strchr(parentName, '%');
	if (rad && (!strnicmp(rad, "%5c", 3) || !strnicmp(rad, "%05c", 4) || !strnicmp(rad, "%2f", 3)  || !strnicmp(rad, "%02f", 4))) {
		char *the_path = gf_strdup(parentName);
		i=0;
		while (1) {
			if (the_path[i]==0) break;
			if (!strnicmp(the_path+i, "%5c", 3) || !strnicmp(the_path+i, "%2f", 3) ) {
				the_path[i] = '/';
				memmove(the_path+i+1, the_path+i+3, strlen(the_path+i+3)+1);
			}
			else if (!strnicmp(the_path+i, "%05c", 4) || !strnicmp(the_path+i, "%02f", 4) ) {
				the_path[i] = '/';
				memmove(the_path+i+1, the_path+i+4, strlen(the_path+i+4)+1);
			}
			i++;
		}
		name = gf_url_concatenate(the_path, pathName);
		gf_free(the_path);
		return name;
	}


	pathSepCount = 0;
	name = NULL;
	if (pathName[0] == '.') {
		if (!strcmp(pathName, "..")) {
			pathSepCount = 1;
			name = "";
		}
		if (!strcmp(pathName, "./")) {
			pathSepCount = 0;
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
	while (strchr(" \r\n\t", tmp[strlen(tmp)-1])) {
		tmp[strlen(tmp)-1] = 0;
	}

	/*remove the last /*/
	for (i = (u32) strlen(parentName); i > 0; i--) {
		//break our path at each separator
		if ((parentName[i-1] == GF_PATH_SEPARATOR) || (parentName[i-1] == '/'))  {
			tmp[i-1] = 0;
			if (!pathSepCount) break;
			pathSepCount--;
		}
	}
	//if i==0, the parent path was relative, just return the pathName
	if (!i) {
		tmp[i] = 0;
		while (pathSepCount) {
			strcat(tmp, "../");
			pathSepCount--;
		}
	} else {
		strcat(tmp, "/");
	}

	i = (u32) strlen(tmp);
	outPath = (char *) gf_malloc(i + strlen(name) + 1);
	sprintf(outPath, "%s%s", tmp, name);

	/*cleanup paths sep for win32*/
	for (i = 0; i<strlen(outPath); i++) 
		if (outPath[i]=='\\') outPath[i] = '/';

check_spaces:
	i=0;
	while (outPath[i]) {
		if (outPath[i] == '?') break;

		if (outPath[i] != '%') {
			i++;
			continue;
		}
		if (!strnicmp(outPath+i, "%3f", 3)) break;
		if (!strnicmp(outPath+i, "%20", 3)) {
			outPath[i]=' ';
			memmove(outPath + i+1, outPath+i+3, strlen(outPath+i)-2);
		}
		i++;
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

char *gf_url_percent_encode(const char *path)
{
	char *outpath, *sep;
	u32 count;
	if (!path) return NULL;

	sep = strchr(path, ' ');
	if (!sep) return gf_strdup(path);
	count = 1;
	sep ++;
	while (1) {
		sep = strchr(sep, ' ');
		if (!sep) break;
        sep ++;
		count ++;
		sep++;
	}
	outpath = gf_malloc(sizeof(char) * (strlen(path) + 2*count + 1));
	strcpy(outpath, path);
	while (1) {
		sep = strchr(outpath, ' ');
		if (!sep) break;
		memmove(sep+3, sep+1, strlen(sep+1)+1);
		sep[0] = '%';
		sep[1] = '2';
		sep[2] = '0';
	}
	return outpath;
}

GF_EXPORT
const char *gf_url_get_resource_name(const char *sURL)
{
	char *sep;
	if (!sURL) return NULL;
	sep = strrchr(sURL, '/');
	if (!sep) sep = strrchr(sURL, '\\');
	if (sep) return sep+1;
	return sURL;
}

GF_EXPORT
Bool gf_url_get_resource_path(const char *sURL, char *res_path)
{
	char *sep;
	strcpy(res_path, sURL);
	sep = strrchr(res_path, '/');
	if (!sep) sep = strrchr(res_path, '\\');
	if (sep) {
		sep[1] = 0;
		return 1;
	}
	return 0;
}


GF_EXPORT
Bool gf_url_remove_last_delimiter(const char *sURL, char *res_path)
{
	strcpy(res_path, sURL);
	if (sURL[strlen(sURL)-1] == GF_PATH_SEPARATOR){
		res_path[strlen(sURL)-1] = 0;
		return GF_TRUE;
	}

	return GF_FALSE;
}

GF_EXPORT
const char* gf_url_get_ressource_extension(const char *sURL){
	const char *dot = strrchr(sURL, '.');
    if(!dot || dot == sURL) return "";
    return dot + 1;
}