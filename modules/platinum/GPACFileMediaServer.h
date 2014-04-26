/*
 *			GPAC - Multimedia Framework C SDK
 *
 *			Authors: Jean Le Feuvre
 *			Copyright (c) Telecom ParisTech 2009-2012
 *			All rights reserved
 *
 *  This file is part of GPAC / Platinum UPnP module
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
 *
 *	----------------------------------------------------------------------------------
 *		PLATINUM IS LICENSED UNDER GPL or commercial agreement - cf platinum license
 *	----------------------------------------------------------------------------------
 *
 */


#ifndef _GPAC_FILE_MEDIA_SERVER_H_
#define _GPAC_FILE_MEDIA_SERVER_H_

#include "PltFileMediaServer.h"

#include <gpac/list.h>

#define MAX_PATH_LENGTH 1024

class PLT_MetadataHandler;

class GPAC_MediaDirectory
{
public:
	/*
	if is_hidden is set, directory is not visible during BrowseDirectChildren
	if alias is NULL, a CRC32 of the path name will be used
	*/
	GPAC_MediaDirectory(const char *alias, const char *path, Bool is_hidden = GF_FALSE)
	{
		m_Path = path;
		m_Path.Replace('/', NPT_FilePath::Separator);
		m_Path.TrimRight("/\\");
		m_Path += NPT_FilePath::Separator;
		m_Alias = alias;
		m_Alias.TrimRight("/\\");
		m_Alias.TrimLeft("/\\");
		m_Hide = is_hidden;
	}
	NPT_String m_Path;
	NPT_String m_Alias;
	Bool m_Hide;
};

class GPAC_VirtualFile
{
public:
	GPAC_VirtualFile(const char *uri="", const char *val="", const char *mime="", Bool temporary= GF_FALSE)
	{
		m_URI = uri;
		m_Content = val;
		m_MIME = mime;
		m_temporary = temporary;
	}
	bool operator==(const GPAC_VirtualFile & v1) {
		return m_URI==v1.m_URI;
	}

	NPT_String m_URI;
	NPT_String m_Content;
	NPT_String m_MIME;
	Bool m_temporary;
};

class GPAC_FileMediaServer : public PLT_FileMediaServer
{
public:
	GPAC_FileMediaServer(const char*  friendly_name,
	                     bool         show_ip = false,
	                     const char*  uuid = NULL,
	                     NPT_UInt16   port = 0);


	void AddSharedDirectory(const char *path, const char *alias, Bool is_hidden = GF_FALSE);

	NPT_String GetResourceURI(const char *file_path, const char *for_host);
	void ShareVirtualResource(const char *res_uri, const char *res_val, const char *res_mime, Bool temporary = GF_FALSE);

protected:
	virtual NPT_Result OnBrowseDirectChildren(PLT_ActionReference&          action,
	        const char*                   object_id,
	        const char*                   filter,
	        NPT_UInt32                    starting_index,
	        NPT_UInt32                    requested_count,
	        const char *   sort_criteria,
	        const PLT_HttpRequestContext& context);

	virtual NPT_Result GetFilePath(const char* object_id, NPT_String& filepath);

	virtual NPT_Result ServeFile(NPT_HttpRequest&              request,
	                             const NPT_HttpRequestContext& context,
	                             NPT_HttpResponse&             response,
	                             const NPT_String&             file_path);

	virtual PLT_MediaObject* BuildFromFilePath(const NPT_String&             filepath,
	        const PLT_HttpRequestContext& context,
	        bool                          with_count = true,
	        bool                          keep_extension_in_title = false);

	PLT_MediaObject* BuildFromFilePathAndHost(const NPT_String&        filepath,
	        const PLT_HttpRequestContext *context = NULL,
	        bool                     with_count = true,
	        bool                     keep_extension_in_title = false,
	        const char *for_host = NULL);

	NPT_Result ServeVirtualFile(NPT_HttpResponse& response,
	                            GPAC_VirtualFile  *vfile,
	                            NPT_Position      start,
	                            NPT_Position      end,
	                            bool              request_is_head);

private:
	NPT_List<GPAC_MediaDirectory> m_Directories;
	NPT_List<GPAC_VirtualFile> m_VirtualFiles;
};

#endif /* _PLT_FILE_MEDIA_SERVER_H_ */
