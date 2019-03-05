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
	GF_URL_TYPE_FILE_PATH = 0,

	/*absolute file:// URI */
	GF_URL_TYPE_FILE_URI,

	/*relative path or URL*/
	GF_URL_TYPE_RELATIVE ,

	/*any other absolute URI*/
	GF_URL_TYPE_ANY_URI,

	/*invalid input */
	GF_URL_TYPE_INVALID,
};

/*resolve the protocol type, for a std URL: http:// or ftp:// ...*/
static u32 URL_GetProtocolType(const char *pathName)
{
	char *begin;
	if (!pathName) return GF_URL_TYPE_INVALID;

	/* URL with the data scheme are not relative to avoid concatenation */
	if (!strnicmp(pathName, "data:", 5)) return GF_URL_TYPE_ANY_URI;


	//conditions for a file path to be absolute:
	// - on posix: absolute iif starts with '/'
	// - on windows: absolute if
	//	* starts with \ or / (current drive)
	//	* OR starts with <LETTER>: and then \ or /
	//	* OR starts with \\host\share\<path> [NOT HANDLED HERE]
#ifndef WIN32
	if (pathName[0] == '/')
#else
	if ( (pathName[0] == '/') || (pathName[0] == '\\')
		|| ( strlen(pathName)>2 && pathName[1]==':'
			&& ((pathName[2] == '/') || (pathName[2] == '\\'))
		   )
	   )
#endif
		return GF_URL_TYPE_FILE_PATH;


	begin = strstr(pathName, "://");
	if (!begin)
		return GF_URL_TYPE_RELATIVE;

	else if (!strnicmp(pathName, "file://", 7))
		return (strlen(pathName)>7 ? GF_URL_TYPE_FILE_URI : GF_URL_TYPE_INVALID);

	return GF_URL_TYPE_ANY_URI;
}

/*gets protocol type*/
GF_EXPORT
Bool gf_url_is_local(const char *pathName)
{
	u32 mode = URL_GetProtocolType(pathName);
	return (mode!=GF_URL_TYPE_INVALID && mode!=GF_URL_TYPE_ANY_URI) ? GF_TRUE : GF_FALSE;
}

GF_EXPORT
char *gf_url_get_absolute_path(const char *pathName, const char *parentPath)
{
	char* sep;
	u32 parent_type;
	char* res = NULL;

	u32 prot_type = URL_GetProtocolType(pathName);

	switch (prot_type) {

		// if it's already absolute, do nothing
		case GF_URL_TYPE_FILE_PATH:
		case GF_URL_TYPE_ANY_URI:
			res = gf_strdup(pathName);
			break;

		// if file URI, remove the scheme part
		case GF_URL_TYPE_FILE_URI:

			pathName += 6; // keep a slash in case it's forgotten

			/* Windows file URIs SHOULD BE in the form "file:///C:\..."
			 * Unix file URIs SHOULD BE in the form "file:///home..."
			 * anything before the 3rd '/' is a hostname
			*/
			sep = strchr(pathName+1, '/');
			if (sep) {
				pathName = sep;

				// dirty way to say if windows
				// consume the third / in that case
				if (strlen(pathName) > 2 && pathName[2]==':')
					pathName++;
			}
			res = gf_strdup(pathName);
			break;

		// if it's relative, it depends on the parent
		case GF_URL_TYPE_RELATIVE:
			parent_type = URL_GetProtocolType(parentPath);

			// in this case the parent is of no help to find an absolute path so we do nothing
			if (parent_type == GF_URL_TYPE_RELATIVE || parent_type == GF_URL_TYPE_INVALID )
				res = gf_strdup(pathName);
			else
				res = gf_url_concatenate(parentPath, pathName);

			break;

	}

	return res;

}


GF_EXPORT
char *gf_url_concatenate(const char *parentName, const char *pathName)
{
	u32 pathSepCount, i, prot_type;
	char *outPath, *name, *rad, *tmp2;
	char tmp[GF_MAX_PATH];

	if (!pathName && !parentName) return NULL;
	if (!pathName) return gf_strdup(parentName);
	if (!parentName) return gf_strdup(pathName);

	if (!strncmp(pathName, "data:", 5)) return gf_strdup(pathName);

	if ((strlen(parentName) > GF_MAX_PATH) || (strlen(pathName) > GF_MAX_PATH)) {
		GF_LOG(GF_LOG_ERROR, GF_LOG_CORE, ("URL too long for concatenation: \n%s\n", pathName));
		return NULL;
	}

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
		outPath = (char*)gf_malloc(strlen(parentName) + strlen(name) + 2);
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
	//strip query part or fragment part
	rad = strchr(tmp, '?');
	if (rad) rad[0] = 0;
	tmp2 = strrchr(tmp, '/');
	if (!tmp2) tmp2 = strrchr(tmp, '\\');
	if (!tmp2) tmp2 = tmp;
	rad = strchr(tmp2, '#');
	if (rad) rad[0] = 0;

	/*remove the last /*/
	for (i = (u32) strlen(tmp); i > 0; i--) {
		//break our path at each separator
		if ((tmp[i-1] == GF_PATH_SEPARATOR) || (tmp[i-1] == '/'))  {
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

//TODO handle reserved characters
const char *pce_special = " %";
const char *pce_encoded = "0123456789ABCDEF";

char *gf_url_percent_encode(const char *path)
{
	char *outpath;
	u32 i, count, len;
	if (!path) return NULL;

	len = (u32) strlen(path);
	count = 0;
	for (i=0; i<len; i++) {
		u8 c = path[i];
		if (strchr(pce_special, c) != NULL) {
			if ((i+2<len) && ((strchr(pce_encoded, path[i+1]) == NULL) || (strchr(pce_encoded, path[i+2]) == NULL))) {
				count+=2;
			}
		} else if (c>>7) {
			count+=2;
		}
	}
	if (!count) return gf_strdup(path);
	outpath = (char*)gf_malloc(sizeof(char) * (len + count + 1));
	strcpy(outpath, path);

	count = 0;
	for (i=0; i<len; i++) {
		Bool do_enc = GF_FALSE;
		u8 c = path[i];

		if (strchr(pce_special, c) != NULL) {
			if ((i+2<len) && ((strchr(pce_encoded, path[i+1]) == NULL) || (strchr(pce_encoded, path[i+2]) == NULL))) {
				do_enc = GF_TRUE;
			}
		} else if (c>>7) {
			do_enc = GF_TRUE;
		}

		if (do_enc) {
			char szChar[3];
			sprintf(szChar, "%02X", c);
			outpath[i+count] = '%';
			outpath[i+count+1] = szChar[0];
			outpath[i+count+2] = szChar[1];
			count+=2;
		} else {
			outpath[i+count] = c;
		}
	}
	outpath[i+count] = 0;
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
		return GF_TRUE;
	}
	return GF_FALSE;
}


GF_EXPORT
Bool gf_url_remove_last_delimiter(const char *sURL, char *res_path)
{
	strcpy(res_path, sURL);
	if (sURL[strlen(sURL)-1] == GF_PATH_SEPARATOR) {
		res_path[strlen(sURL)-1] = 0;
		return GF_TRUE;
	}

	return GF_FALSE;
}

GF_EXPORT
const char* gf_url_get_ressource_extension(const char *sURL) {
	const char *dot = strrchr(sURL, '.');
	if(!dot || dot == sURL) return "";
	return dot + 1;
}
